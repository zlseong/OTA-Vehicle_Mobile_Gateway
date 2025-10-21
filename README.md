# Vehicle Mobile Gateway v2.0

Simple HTTPS client for Linux Gateway connecting to OTA server.

## Features

- ✅ HTTPS communication with server
- ✅ Device registration
- ✅ Status updates
- ✅ Health monitoring
- ✅ SSL certificate verification control

## Build

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
- `server.host`: Server IP address
- `server.port`: Server port
- `device.type`: Device type (Linux, TC375, etc.)
- `tls.verify_peer`: Enable/disable SSL verification

## Server Info

- Host: 54.234.98.110
- Port: 8765
- Protocol: HTTPS
- API Base: /api

