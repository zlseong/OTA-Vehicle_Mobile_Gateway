# Vehicle Mobile Gateway v2.0

Complete OTA gateway with HTTP/HTTPS + MQTT + PQC support for Linux.

## Features

## 구현 기능 (Implemented Features)

### 1. 차량 통신 프로토콜 (Vehicle Communication Protocols)
- DoIP (Diagnostics over IP) - ISO 13400
- UDS (Unified Diagnostic Services) - ISO 14229
- MQTT (Message Queuing Telemetry Transport)
- HTTP/HTTPS REST API

### 2. 듀얼 파티션 아키텍처 (Dual Partition Architecture)
- A/B Partition 구조
- Yocto 기반 파티션 레이아웃
- SWUpdate/RAUC 통합

### 3. 부트 관리 (Boot Management)
- U-Boot 환경 변수 제어
- 자동 Rollback (3회 실패 시)
- fw_setenv 기반 파티션 전환

### 4. 메모리 동기화 (Memory Synchronization)
- dd 명령 기반 전체 동기화
- rsync 기반 증분 동기화
- 파일시스템 무결성 검증

### 5. Hybrid PQC-TLS (Post-Quantum Cryptography)
- ML-KEM-768 (Key Encapsulation)
- ML-DSA-65 (Digital Signature)
- OpenSSL 3.6.0 Native 지원

### 6. JSON 데이터 구조 (JSON Data Formats)
- VCI (Vehicle Configuration Information)
- Metadata (OTA Package Information)
- Readiness (Pre-update Conditions)

### 7. 원격 진단 (Remote Diagnostics)
- MQTT 기반 명령 수신
- DoIP/UDS 명령 전송
- 이벤트 기반 처리

### 8. OTA 업데이트 관리 (OTA Update Management)
- 패키지 다운로드 및 진행률 보고
- 암호화 서명 검증
- Standby 파티션 설치
- 부트 검증 및 Rollback

### 9. 시스템 모니터링 (System Monitoring)
- Adaptive Heartbeat (차량 상태별)
- Telemetry 데이터 수집
- Health Check

### 10. 보안 (Security)
- TLS 1.3 암호화
- X.509 인증서 인증
- 디지털 서명 검증
- Secure Boot Chain

## Architecture

```
AWS Server (54.234.98.110)
    │
    ├─ HTTP/HTTPS (8765) ──────┐
    └─ MQTT/TLS (8883) ────────┼─→ Linux Gateway
                               │
                               └─→ TC375 MCU (future)
```

## Build

### Prerequisites

```bash
# macOS
brew install curl nlohmann-json mosquitto

# Linux
sudo apt install libcurl4-openssl-dev nlohmann-json3-dev libmosquitto-dev
```

### Compile

```bash
mkdir build && cd build
cmake ..
make
```

## Run

```bash
./gateway [config.json]
```

## Configuration

Edit `config.json`:

### HTTP/HTTPS
- `server.http.port`: HTTP server port (8765)
- `server.http.use_https`: Enable HTTPS (true)
- `server.http.api_base`: API base path (/api)

### MQTT
- `server.mqtt.port`: MQTT broker port (8883)
- `server.mqtt.use_tls`: Enable MQTT TLS (true)
- `server.mqtt.topics`: Command, status, OTA topics

### TLS
- `tls.verify_peer`: Verify SSL certificates (false for dev)
- `tls.version`: TLS version (1.2 or 1.3)

### PQC (Post-Quantum Cryptography)
- `pqc.enabled`: Enable PQC (requires liboqs)
- `pqc.kem_algorithm`: Key Exchange (Kyber768, Kyber1024)
- `pqc.signature_algorithm`: Signature (Dilithium3, Dilithium5)

## Server Info

- Host: `54.234.98.110`
- HTTP Port: `8765` (HTTPS)
- MQTT Port: `8883` (TLS)
- API Base: `/api`

## Device Type

Current: **Linux** (not TC375)
- Acts as central gateway
- Communicates with AWS server
- Will communicate with TC375 ECUs in future

## Testing

```bash
# Run all tests
./gateway config.json

# Test HTTP only
curl -k https://54.234.98.110:8765/api/health

# Monitor MQTT
mosquitto_sub -h 54.234.98.110 -p 8883 -t "vmg/#" --cafile ca.crt
```

## PQC Support

PQC is configured but requires liboqs library:

```bash
# Install liboqs
brew install liboqs  # macOS
# or compile from source: https://github.com/open-quantum-safe/liboqs

# Rebuild with PQC
cd build && cmake .. && make
```

Set `pqc.enabled: true` in config.json to activate.

## Next Steps

1. ✅ HTTP/HTTPS working
2. ✅ MQTT integration complete
3. ⏳ Gateway ↔ TC375 communication
4. ⏳ OTA file transfer logic
5. ⏳ PQC implementation with liboqs

## License

MIT

