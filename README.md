# Vehicle Mobile Gateway v2.0

Complete OTA gateway with HTTP/HTTPS + MQTT + PQC support for Linux.

## Features

- ✅ **HTTP/HTTPS** - RESTful API communication with TLS 1.2/1.3
- ✅ **MQTT** - Real-time messaging with TLS support
- ✅ **Device Management** - Registration, status updates, health monitoring
- ✅ **SSL/TLS Control** - Certificate verification control for development
- ✅ **PQC-Ready** - Post-Quantum Cryptography configuration (requires liboqs)

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

