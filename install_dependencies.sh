#!/bin/bash
# VMG 의존성 설치 스크립트 (Ubuntu/WSL)

echo "================================================================"
echo "  VMG 의존성 설치 (Ubuntu/Debian/WSL)"
echo "================================================================"
echo ""

# 패키지 리스트 업데이트
echo "[1/5] Updating package lists..."
sudo apt update

# 기본 빌드 도구
echo ""
echo "[2/5] Installing build tools..."
sudo apt install -y build-essential cmake git

# OpenSSL
echo ""
echo "[3/5] Installing OpenSSL..."
sudo apt install -y libssl-dev

# libcurl
echo ""
echo "[4/5] Installing libcurl..."
sudo apt install -y libcurl4-openssl-dev

# nlohmann-json
echo ""
echo "[5/5] Installing nlohmann-json..."
sudo apt install -y nlohmann-json3-dev

# Paho MQTT C++
echo ""
echo "[6/7] Installing Paho MQTT C..."
sudo apt install -y libpaho-mqtt-dev

echo ""
echo "[7/7] Installing Paho MQTT C++..."
sudo apt install -y libpaho-mqttpp-dev

echo ""
echo "================================================================"
echo "  ✓ 모든 의존성 설치 완료!"
echo "================================================================"
echo ""
echo "다음 명령으로 빌드하세요:"
echo "  cd VMG"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  make -j8"
echo ""

