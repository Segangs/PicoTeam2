import paho.mqtt.client as mqtt
import requests
import json
import os
import threading
from datetime import datetime, timezone, timedelta
from flask import Flask, jsonify, render_template, request

# --- 설정 ---
TELEGRAM_TOKEN = "****" # 비공개 처리 26.4.6.
CHAT_ID = "****"  # 비공개 처리 26.4.6.
MQTT_BROKER = "localhost"
TOPIC = "sensor/data"
DB_FILE = "sensor_db.json"

app = Flask(__name__)
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)

# 한국 시간(KST) 설정
KST = timezone(timedelta(hours=9))

# --- 텔레그램 전송 함수 (바로가기 버튼 추가) ---
def send_telegram(message):
    url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
    
    # 텔레그램 메시지 하단에 달릴 버튼 생성
    keyboard = {
        "inline_keyboard": [[
            {"text": "📊 텃밭 대시보드", "url": "http://***.***.***.***:5000"}. # 비공개 처리 26.4.6.
        ]]
    }
    
    params = {
        "chat_id": CHAT_ID, 
        "text": message,
        "reply_markup": json.dumps(keyboard) # 버튼 데이터 첨부
    }
    try:
        requests.get(url, params=params)
    except Exception as e:
        print(f"Telegram error: {e}")

# --- 피코 데이터 가공 및 DB 저장 ---
def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        data = json.loads(payload)
        
        # 1. 수신 시간 기록 (반드시 KST 기준)
        now = datetime.now(KST)
        data['timestamp'] = now.strftime("%Y-%m-%d %H:%M:%S")
        data['time_short'] = now.strftime("%H:%M")
        
        # 2. DB 저장
        db_data = []
        if os.path.exists(DB_FILE):
            with open(DB_FILE, 'r', encoding='utf-8') as f:
                db_data = json.load(f)
        db_data.append(data)
        with open(DB_FILE, 'w', encoding='utf-8') as f:
            json.dump(db_data[-1000:], f, ensure_ascii=False, indent=2)

        # 3. 텔레그램 전송 로직
        event = data.get('event')
        temp, humi = data.get('temp'), data.get('humi')
        water, light_raw = data.get('water'), data.get('light')
        shade, soil_raw = data.get('shade'), data.get('soil')

        light_str = "적절"
        if light_raw >= 3950: light_str = "과다"  
        elif light_raw < 3500: light_str = "부족" 

        shade_str = "ON" if shade == 1 else "OFF"
        soil_value = 4095 - soil_raw
        soil_str = "건조" if soil_value <= 1100 else "적절" 

        tg_msg = ""
        if event == "regular":
            tg_msg = (f"📢 (정기 알림) 현재 텃밭 상황을 알려드려요!\n"
                      f"🌡️ 현재 온도 : {temp}도\n💧 현재 습도 : {humi}%\n"
                      f"☀️ 현재 광량 : {light_str}\n🪴 차양막 상태 : {shade_str}\n"
                      f"🌊 물탱크 잔량 : {water}%\n🌱 토양 수분 상태 : {soil_str}")
        elif event == "water_low":
            tg_msg = f"⚠️ 현재 물탱크 잔량 {water}%입니다. 물탱크에 물을 채워주세요.\n(온도: {temp}도, 습도: {humi}%)"
        elif event == "shade_close":
            tg_msg = f"☀️ 햇빛이 강하여 차양막으로 식물을 보호합니다.\n(온도: {temp}도, 습도: {humi}%)"
        elif event == "shade_open":
            tg_msg = f"🪴 차양막을 열었습니다.\n(온도: {temp}도, 습도: {humi}%)"
        elif event == "pump_on":
            tg_msg = f"💧 건조한 텃밭에 물을 주었습니다.\n(온도: {temp}도, 습도: {humi}%)"

        if tg_msg:
            send_telegram(tg_msg)

    except Exception as e:
        print(f"데이터 처리 에러: {e}")

def start_mqtt():
    global client
    client.on_message = on_message
    client.connect(MQTT_BROKER, 1883, 60)
    client.subscribe(TOPIC)
    client.loop_forever()

# --- 웹 대시보드 라우팅 ---
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/data')
def get_data():
    if os.path.exists(DB_FILE):
        with open(DB_FILE, 'r', encoding='utf-8') as f:
            return jsonify(json.load(f))
    return jsonify([])

@app.route('/api/control/<command>', methods=['POST'])
def control_device(command):
    client.publish("sensor/control", command)
    print(f"👉 원격 제어 명령 발송: {command}")
    return jsonify({"status": "success", "command": command})

if __name__ == '__main__':
    mqtt_thread = threading.Thread(target=start_mqtt, daemon=True)
    mqtt_thread.start()
    print("🌐 웹 대시보드 서버 시작: http://*.*.*.*:5000") # 비공개 처리 26.4.6.
    app.run(host='0.0.0.0', port=5000)
