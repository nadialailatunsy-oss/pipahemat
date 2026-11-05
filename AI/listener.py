import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"   # ganti kalau broker di kodemu berbeda
PORT = 1883
TOPIK = "pipahemat/kelompok_4/#"   # tanda # = semua topic di bawahnya

def on_connect(client, userdata, flags, reason_code, properties):
    print("Terhubung ke broker, kode:", reason_code)
    client.subscribe(TOPIK)

def on_message(client, userdata, msg):
    print(msg.topic, "->", msg.payload.decode())

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.loop_forever()