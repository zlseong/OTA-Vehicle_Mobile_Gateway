# VMG Build and Test Guide

## ✅ 완료된 작업

### **1. MQTT Client (Paho MQTT C++)**
- ✅ 실제 Paho MQTT C++ 라이브러리 사용
- ✅ OTA-Server API 토픽 형식 구현 (`oem/{vin}/*`)
- ✅ Message types: `vehicle_wake_up`, `vci_report`, `ota_readiness_response`, etc.
- ✅ 자동 재연결, QoS 지원

### **2. HTTP Client (libcurl)**
- ✅ GET/POST 메소드 구현
- ✅ JSON 데이터 전송
- ✅ 파일 다운로드 (OTA 패키지)
- ✅ Resume 지원 (Range header)
- ✅ Bearer Token 인증

### **3. 서버 통합**
- ✅ OTA-Server MQTT API 완벽 호환
- ✅ OTA-Server HTTP API 완벽 호환
- ✅ VIN 기반 토픽 생성
- ✅ Wake-up, VCI, Readiness 메시지 전송

---

## 📦 의존성 설치

### **Ubuntu/Debian (WSL 포함)**

```bash
# 기본 빌드 도구
sudo apt update
sudo apt install -y build-essential cmake git

# 라이브러리
sudo apt install -y \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev
```

### **macOS (Homebrew)**

```bash
brew install cmake openssl curl nlohmann-json mosquitto
brew install paho-mqtt-cpp
```

---

## 🔨 빌드 방법

```bash
cd VMG
mkdir build && cd build

# CMake 구성
cmake ..

# 빌드 (8코어 병렬)
make -j8

# 결과 확인
ls -lh vmg
```

### **예상 출력:**
```
-- The CXX compiler identification is GNU 11.4.0
-- Found OpenSSL: 3.0.2
-- Found CURL: 7.81.0
-- Found nlohmann_json: 3.11.2
-- Found Paho MQTT C: /usr/lib/libpaho-mqtt3as.so
-- Found Paho MQTT C++: /usr/lib/libpaho-mqttpp3.so
-- Configuring done
-- Generating done
-- Build files have been written to: /path/to/VMG/build

[ 11%] Building CXX object CMakeFiles/vmg.dir/main.cpp.o
[ 22%] Building CXX object CMakeFiles/vmg.dir/src/app/config_manager.cpp.o
...
[100%] Linking CXX executable vmg
[100%] Built target vmg
```

---

## 🧪 테스트 방법

### **1. 서버 시작 (별도 터미널)**

```bash
cd OTA-Server/server
pip3 install -r requirements.txt
python3 server/app.py
```

**서버 출력:**
```
╔════════════════════════════════════════════════════════════╗
║        PQC OTA Server - OEM Cloud                         ║
║        Zonal E/E Architecture                              ║
╚════════════════════════════════════════════════════════════╝

[MQTT] Connected to broker at localhost:1883
[Flask] Starting HTTPS server on port 5000...
 * Running on http://0.0.0.0:5000
```

---

### **2. config.json 수정**

```json
{
  "server": {
    "host": "localhost",  // 또는 실제 서버 IP
    "http": {
      "port": 5000,
      "use_https": false  // 테스트용
    },
    "mqtt": {
      "port": 1883,
      "use_tls": false    // 테스트용
    }
  },
  "vehicle": {
    "vin": "KMHXX00XXXX000001"
  },
  "device": {
    "id": "vmg-test-001"
  }
}
```

---

### **3. VMG 실행**

```bash
cd VMG/build
./vmg ../config.json
```

**예상 출력:**
```
================================================================
  Vehicle Mobile Gateway v2.0
  Hybrid PQC-TLS + DoIP/UDS + HTTP/MQTT
================================================================

[INIT] Loading configuration from config.json...
[INIT] ✓ Configuration loaded
       Device: vmg-test-001 (VIN: KMHXX00XXXX000001)
       Server: localhost:5000
       PQC: Disabled
       Heartbeat: 300s (Adaptive)

[INIT] Initializing HTTP client...
[INIT] ✓ HTTP client initialized

[INIT] Initializing MQTT client...
[MQTT] Connecting to localhost:1883...
[MQTT] ✓ Connected successfully
[INIT] ✓ MQTT client initialized

[CONN] Testing HTTP connection...
[HTTP] GET http://localhost:5000/health → 200 (45 bytes)
[CONN] ✓ HTTP connected

[CONN] ✓ MQTT connected

[CONN] ✓ Subscribed to oem/KMHXX00XXXX000001/command
[CONN] ✓ Subscribed to oem/KMHXX00XXXX000001/ota/campaign
[CONN] ✓ Subscribed to oem/KMHXX00XXXX000001/ota/metadata

[INIT] ✓ All subsystems initialized

================================================================
  ✓ VMG Initialized Successfully!
================================================================

[BOOT] Performing power-on sequence...
[MQTT] Published to oem/KMHXX00XXXX000001/wake_up (234 bytes)
[BOOT] ✓ Vehicle wake-up sent
[BOOT] Collecting VCI...
[VCI] Starting VCI collection (trigger: power_on)...
[HTTP] POST http://localhost:5000/api/vci/upload → 200 (28 bytes)
[VCI] ✓ VCI uploaded to server

================================================================
  ✓ VMG Started Successfully!
  Waiting for external commands...
================================================================

[MAIN] Entering main loop (Press Ctrl+C to exit)...

[HB] ♥ Heartbeat published (state: PARKED_IGNITION_OFF)
[HB] ♥ Heartbeat published (state: PARKED_IGNITION_OFF)
...
```

---

### **4. 서버에서 명령 보내기**

**서버 콘솔에서 VCI 요청:**
```python
# Python shell에서
from mqtt_broker import OTAMQTTBroker
broker = OTAMQTTBroker('localhost', 1883)
broker.connect()
broker.request_vci('KMHXX00XXXX000001')
```

**VMG 출력:**
```
[MQTT] Message received on topic: oem/KMHXX00XXXX000001/command

[VCI] External VCI collection requested
[VCI] Starting VCI collection (trigger: external_request)...
[HTTP] POST http://localhost:5000/api/vci/upload → 200 (28 bytes)
[VCI] ✓ VCI uploaded to server
```

---

## 🔍 트러블슈팅

### **1. MQTT 연결 실패**
```
[MQTT] Connection error: Connection refused
```

**해결:**
```bash
# Mosquitto 브로커 설치 및 시작
sudo apt install mosquitto
sudo systemctl start mosquitto
```

---

### **2. HTTP 연결 실패**
```
[HTTP] Request failed: Couldn't connect to server
```

**해결:**
- 서버가 실행 중인지 확인
- `config.json`에서 `host`와 `port` 확인
- 방화벽 확인

---

### **3. 빌드 에러: Paho MQTT not found**
```
CMake Error: Could not find paho-mqttpp3
```

**해결:**
```bash
# Ubuntu/WSL
sudo apt install libpaho-mqttpp-dev

# macOS
brew install paho-mqtt-cpp
```

---

### **4. 빌드 에러: CURL not found**
```
CMake Error: Could not find CURL
```

**해결:**
```bash
# Ubuntu/WSL
sudo apt install libcurl4-openssl-dev

# macOS
brew install curl
```

---

## 📊 테스트 체크리스트

- [ ] HTTP health check 성공
- [ ] MQTT 연결 성공
- [ ] Wake-up 메시지 전송
- [ ] VCI 수집 및 업로드
- [ ] Readiness 체크 (외부 요청)
- [ ] Heartbeat 주기적 전송 (300초)
- [ ] MQTT 명령 수신 처리
- [ ] Graceful shutdown (Ctrl+C)

---

## 🚀 다음 단계

1. **DoIP/UDS 통합**: ZGW와 실제 통신
2. **PQC-TLS 활성화**: ML-KEM768 + ML-DSA65
3. **OTA 패키지 다운로드 테스트**
4. **듀얼 파티션 업데이트 로직**

---

## 📝 참고 링크

- [OTA-Server GitHub](https://github.com/zlseong/OTA-Server.git)
- [Paho MQTT C++ Documentation](https://github.com/eclipse/paho.mqtt.cpp)
- [libcurl Documentation](https://curl.se/libcurl/)

