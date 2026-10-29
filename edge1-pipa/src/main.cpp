// PipaHemat - EDGE 1 (ESP32 di pipa distribusi)
// Tugas: ukur debit -> olah jadi moving average (L/menit) -> publish via MQTT
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define KELOMPOK "kelompok_4"          // harus sama di edge2 & dashboard
const char* WIFI_SSID = "Wokwi-GUEST"; // WiFi bawaan Wokwi
const char* WIFI_PASS = "";
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;

#define USE_POT 1                      // 1 = potensiometer (Wokwi), 0 = YF-S201 asli
#if USE_POT
  const int PIN_FLOW = 34;             // pin SIG potensiometer
#else
  const int PIN_FLOW = 27;             // pin sinyal YF-S201
#endif
const float MAX_LPM = 30.0;            // potensiometer penuh = 30 L/menit
const uint32_t SAMPLE_MS = 1000;
const int MA_N = 5;                    // moving average 5 sampel

WiFiClient net;
PubSubClient mqtt(net);
char tTelemetry[96], tStatus[96];

float buf[MA_N];
int idx = 0, cnt = 0;
volatile uint32_t pulses = 0;
void IRAM_ATTR onPulse() { pulses = pulses + 1; }

float readFlowLpm() {
#if USE_POT
  return analogRead(PIN_FLOW) * (MAX_LPM / 4095.0f);
#else
  noInterrupts(); uint32_t p = pulses; pulses = 0; interrupts();
  float hz = p / (SAMPLE_MS / 1000.0f);
  return hz / 7.5f;                    // YF-S201: L/menit = Hz / 7.5
#endif
}

// DATA PROCESSING #1: moving average debit
float movingAvg(float x) {
  buf[idx] = x; idx = (idx + 1) % MA_N;
  if (cnt < MA_N) cnt++;
  float s = 0; for (int i = 0; i < cnt; i++) s += buf[i];
  return s / cnt;
}

float r2(float x) { return roundf(x * 100.0f) / 100.0f; }

void ensureMqtt() {
  static uint32_t lastTry = 0;
  if (mqtt.connected()) return;
  if (lastTry && millis() - lastTry < 3000) return;
  lastTry = millis();
  String cid = "pipahemat-e1-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  // Last Will: kalau ESP32 mati, broker otomatis publish "offline"
  if (mqtt.connect(cid.c_str(), NULL, NULL, tStatus, 0, true, "offline")) {
    mqtt.publish(tStatus, "online", true);
    Serial.println("MQTT terhubung");
  }
}

void setup() {
  Serial.begin(115200);
  snprintf(tTelemetry, sizeof(tTelemetry), "pipahemat/%s/edge/edge1/telemetry", KELOMPOK);
  snprintf(tStatus, sizeof(tStatus), "pipahemat/%s/edge/edge1/status", KELOMPOK);
#if !USE_POT
  pinMode(PIN_FLOW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), onPulse, FALLING);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  Serial.print("Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println(" OK");
  mqtt.setKeepAlive(120);
  mqtt.setSocketTimeout(5);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void loop() {
  ensureMqtt();
  mqtt.loop();
  static uint32_t last = 0;
  if (millis() - last >= SAMPLE_MS) {
    last = millis();
    float raw = readFlowLpm();
    float avg = movingAvg(raw);
    JsonDocument doc;
    doc["flow_lpm"] = r2(avg);
    doc["raw_lpm"] = r2(raw);
    char out[128];
    serializeJson(doc, out);
    if (mqtt.connected()) mqtt.publish(tTelemetry, out);
    Serial.printf("raw=%.2f  avg=%.2f L/menit\n", raw, avg);
  }
}