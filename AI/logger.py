import json, csv, os, time
import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
PORT = 1883
TOPIK = "pipahemat/kelompok_4/edge/edge2/telemetry"
OUT = "dataset_asli.csv"

INTERVAL = 1
SKIP_AWAL = 5
VALID = ("AMAN", "WASPADA", "BOCOR")

mulai = time.time()
terakhir = 0
hitung = {"AMAN": 0, "WASPADA": 0, "BOCOR": 0}

baru = not os.path.exists(OUT)
f = open(OUT, "a", newline="")
w = csv.writer(f)
if baru:
    w.writerow(["debit_pipa", "level_cm", "laju_turun_level", "selisih_neraca",
                "label", "status_firmware", "sim_leak_lpm"])

def on_connect(c, u, fl, rc, p):
    print("Terhubung. Label diambil dari status firmware. Ctrl+C untuk berhenti.")
    c.subscribe(TOPIK)

def on_message(c, u, msg):
    global terakhir
    now = time.time()
    if now - mulai < SKIP_AWAL or now - terakhir < INTERVAL:
        return
    try:
        d = json.loads(msg.payload.decode())
        status = d["status"]
        if status not in VALID:
            return
        row = [d["flow_lpm"], d["level_cm"], d["drop_lpm"], d["leak_lpm"],
               status, status, d.get("sim_leak_lpm", "")]
    except Exception as e:
        print("Payload dilewati:", e)
        return
    w.writerow(row)
    f.flush()
    terakhir = now
    hitung[status] += 1
    print(f"AMAN={hitung['AMAN']}  WASPADA={hitung['WASPADA']}  BOCOR={hitung['BOCOR']}  | {row[:4]} -> {status}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, 60)
try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\nSelesai:", hitung)
finally:
    f.close()