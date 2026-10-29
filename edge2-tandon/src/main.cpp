// PipaHemat - EDGE 2 (ESP32 di tandon): level, neraca air, katup
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

#define KELOMPOK "kelompok_4"          // harus sama dengan edge1 & dashboard
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;

#ifndef LEAK_MIN_MS
#define LEAK_MIN_MS 300000UL           // 5 menit (30000UL untuk tes cepat)
#endif
#ifndef SIM_LEVEL
#define SIM_LEVEL 1                    // 1 = level disimulasikan (demo Wokwi)
#endif

const int PIN_TRIG = 5, PIN_ECHO = 18, PIN_SERVO = 13;
const int SERVO_OPEN = 0, SERVO_CLOSED = 90;

const float TANK_H_CM = 100.0;         // jarak sensor ke dasar tandon
const float AREA_CM2 = 400.0;          // 1 cm = 0.4 liter
const float LEAK_THRESHOLD_LPM = 1.5;  // ambang selisih
const uint32_t GRACE_MS = 15000;       // toleransi lonjakan sesaat
const int WINDOW_S = 20;               // jendela laju turun level (detik)

WiFiClient net;
PubSubClient mqtt(net);
Servo katup;
char tFlow[96], tTelemetry[96], tAlert[96], tCmd[96], tStatus[96];

float flowLpm = 0; uint32_t flowTs = 0;
float lv[WINDOW_S];                    // level (liter) per sampel
float fv[WINDOW_S];                    // debit pipa per sampel (jendela yang sama dengan level)
int n = 0;
float levelCm = 0, levelL = 0, dropLpm = 0, leakLpm = 0, wastedL = 0;
float simOffsetCm = 0, simLeakLpm = 0;
bool valveClosed = false;
uint32_t leakStart = 0, lastLeakSeen = 0;
const char* status = "MENUNGGU_DATA";
bool latched = false;                  // BOCOR di-latch sampai perintah BUKA

float r2(float x) { return roundf(x * 100.0f) / 100.0f; }

// ---------- sensor ----------
float readDistCm() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long d = pulseIn(PIN_ECHO, HIGH, 30000);
  if (d == 0) return NAN;
  return d * 0.0343f / 2.0f;
}
float readDistAvg() {
  float s = 0; int k = 0;
  for (int i = 0; i < 3; i++) { float d = readDistCm(); if (!isnan(d)) { s += d; k++; } delay(30); }
  return k ? s / k : NAN;
}

void setValve(bool close) {
  valveClosed = close;
  katup.write(close ? SERVO_CLOSED : SERVO_OPEN);
}

// ---------- DATA PROCESSING #2: laju penurunan level (L/menit) ----------
void pushLevel(float x, float f) {
  if (n > 0 && x > lv[n - 1] + 1.5f) n = 0;   // level naik = tandon diisi, reset jendela
  if (n < WINDOW_S) { lv[n] = x; fv[n] = f; n++; }
  else {
    memmove(lv, lv + 1, sizeof(float) * (WINDOW_S - 1));
    memmove(fv, fv + 1, sizeof(float) * (WINDOW_S - 1));
    lv[WINDOW_S - 1] = x; fv[WINDOW_S - 1] = f;
  }
}
float calcDropLpm() {
  if (n < WINDOW_S) return 0;
  return (lv[0] - lv[n - 1]) / (n - 1) * 60.0f;  // liter/detik -> L/menit
}
float avgFlow() {                                // debit rata-rata selama jendela yang sama
  if (n < 2) return 0;
  float s = 0;
  for (int i = 1; i < n; i++) s += fv[i];
  return s / (n - 1);
}

// ---------- ALGORITMA: neraca air ----------
void publishAlert() {
  JsonDocument d;
  d["status"] = "BOCOR"; d["leak_lpm"] = r2(leakLpm); d["wasted_l"] = r2(wastedL);
  char out[160]; serializeJson(d, out);
  mqtt.publish(tAlert, out);
}

void evaluate() {
  uint32_t now = millis();
  bool flowOk = flowTs != 0 && (now - flowTs) < 30000;
  if (latched) return;
  if (!flowOk || n < WINDOW_S) { status = "MENUNGGU_DATA"; return; }

  leakLpm = dropLpm - avgFlow();                  // air hilang = turun tandon - debit pipa (jendela sama)
  if (leakLpm > LEAK_THRESHOLD_LPM) {
    if (!leakStart) leakStart = now;
    lastLeakSeen = now;
    wastedL += leakLpm / 60.0f;                   // estimasi air terbuang
  } else if (leakStart && now - lastLeakSeen > GRACE_MS) {
    leakStart = 0;                                // selisih hilang cukup lama -> batal
  }

  if (!leakStart) status = "AMAN";
  else if (now - leakStart >= LEAK_MIN_MS) {
    status = "BOCOR"; latched = true;
    setValve(true);
    publishAlert();
  } else status = "WASPADA";
}

// ---------- MQTT ----------
void onMsg(char* topic, byte* p, unsigned int len) {
  String t(topic), msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)p[i];
  if (t == tFlow) {
    JsonDocument d;
    if (!deserializeJson(d, msg)) { flowLpm = d["flow_lpm"] | 0.0f; flowTs = millis(); }
  } else if (t == tCmd) {
    msg.trim(); msg.toUpperCase();
    if (msg == "TUTUP") setValve(true);
    else if (msg == "BUKA") { setValve(false); latched = false; leakStart = 0; status = "AMAN"; }
    else if (msg == "RESET") wastedL = 0;
    else if (msg.startsWith("SIM:")) simLeakLpm = msg.substring(4).toFloat();
    Serial.println("CMD: " + msg);
  }
}

void ensureMqtt() {
  static uint32_t lastTry = 0;
  if (mqtt.connected()) return;
  if (lastTry && millis() - lastTry < 3000) return;
  lastTry = millis();
  String cid = "pipahemat-e2-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (mqtt.connect(cid.c_str(), NULL, NULL, tStatus, 0, true, "offline")) {
    mqtt.publish(tStatus, "online", true);
    mqtt.subscribe(tFlow);
    mqtt.subscribe(tCmd);
    Serial.println("MQTT terhubung");
  }
}

void publishTelemetry() {
  JsonDocument d;
  d["level_cm"] = r2(levelCm); d["level_l"] = r2(levelL);
  d["drop_lpm"] = r2(dropLpm); d["flow_lpm"] = r2(flowLpm); d["leak_lpm"] = r2(leakLpm);
  d["status"] = status; d["wasted_l"] = r2(wastedL);
  d["valve"] = valveClosed ? "TUTUP" : "BUKA"; d["sim_leak_lpm"] = r2(simLeakLpm);
  char out[320]; serializeJson(d, out);
  mqtt.publish(tTelemetry, out);
}

void setup() {
  Serial.begin(115200);
  snprintf(tFlow, sizeof(tFlow), "pipahemat/%s/edge/edge1/telemetry", KELOMPOK);
  snprintf(tTelemetry, sizeof(tTelemetry), "pipahemat/%s/edge/edge2/telemetry", KELOMPOK);
  snprintf(tAlert, sizeof(tAlert), "pipahemat/%s/alerts", KELOMPOK);
  snprintf(tCmd, sizeof(tCmd), "pipahemat/%s/cmd/katup", KELOMPOK);
  snprintf(tStatus, sizeof(tStatus), "pipahemat/%s/edge/edge2/status", KELOMPOK);
  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_ECHO, INPUT);
  ESP32PWM::allocateTimer(0);
  katup.setPeriodHertz(50); katup.attach(PIN_SERVO, 500, 2400);
  setValve(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  Serial.print("Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println(" OK");
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(120);
  mqtt.setSocketTimeout(5);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMsg);
}

void loop() {
  ensureMqtt();
  mqtt.loop();

  static uint32_t lastSample = 0, lastPub = 0;
  if (millis() - lastSample >= 1000) {
    lastSample = millis();
    float dist = readDistAvg();
    if (isnan(dist)) dist = TANK_H_CM - levelCm;
#if SIM_LEVEL
    if (!valveClosed) simOffsetCm += (flowLpm + simLeakLpm) / 60.0f * 1000.0f / AREA_CM2;
    if (TANK_H_CM - (dist + simOffsetCm) < 10.0f) { simOffsetCm = 0; n = 0; }  // isi ulang
    dist += simOffsetCm;
#endif
    levelCm = constrain(TANK_H_CM - dist, 0.0f, TANK_H_CM);
    levelL = levelCm * AREA_CM2 / 1000.0f;
    pushLevel(levelL, flowLpm);
    dropLpm = calcDropLpm();
    evaluate();
    Serial.printf("n=%d level=%.1f L  turun=%.2f  debit=%.2f  selisih=%.2f  status=%s\n",
                  n, levelL, dropLpm, flowLpm, leakLpm, status);
  }
  if (millis() - lastPub >= 2000 && mqtt.connected()) { lastPub = millis(); publishTelemetry(); }
}