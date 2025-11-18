#!/bin/bash
#
# VMG 원클릭 설치 스크립트 (WSL Ubuntu)
#
# 사용법: bash install_wsl.sh
#

set -e

echo "============================================================"
echo "VMG WSL 환경 자동 설치"
echo "============================================================"
echo ""

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Step 1: 시스템 업데이트
print_step "시스템 업데이트..."
sudo apt update
print_success "시스템 업데이트 완료"

# Step 2: 빌드 도구 설치
print_step "빌드 도구 설치 (gcc, g++, cmake, make)..."
sudo apt install -y build-essential cmake git
print_success "빌드 도구 설치 완료"

# Step 3: 필수 라이브러리 설치
print_step "필수 라이브러리 설치..."
sudo apt install -y \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    zlib1g-dev

print_success "필수 라이브러리 설치 완료"

# Step 4: 버전 확인
echo ""
echo "============================================================"
echo "설치된 도구 버전 확인"
echo "============================================================"
echo "CMake:   $(cmake --version | head -n1)"
echo "GCC:     $(gcc --version | head -n1)"
echo "G++:     $(g++ --version | head -n1)"
echo "Python:  $(python3 --version)"
echo "============================================================"
echo ""

print_success "모든 의존성 설치 완료!"
echo ""
echo "다음 명령으로 빌드하세요:"
echo "  cd test"
echo "  ./build_and_test.sh"
echo ""

