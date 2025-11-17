# VMG 의존성 설치 가이드

## Windows (WSL Ubuntu)

### 방법 1: 자동 설치 스크립트

```bash
# WSL에서 VMG 디렉토리로 이동
cd /mnt/c/Users/user/AURIX-v1.10.24-workspace/OTA-Zonal_Gateway/VMG

# 스크립트 실행 권한 부여
chmod +x install_dependencies.sh

# 스크립트 실행
./install_dependencies.sh
```

### 방법 2: 수동 설치

```bash
# 패키지 업데이트
sudo apt update

# 빌드 도구
sudo apt install -y build-essential cmake git

# 라이브러리
sudo apt install -y \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev
```

---

## 설치 확인

```bash
# CMake 버전 확인
cmake --version
# 출력: cmake version 3.xx.x

# 패키지 확인
dpkg -l | grep paho
# 출력:
# libpaho-mqtt-dev
# libpaho-mqttpp-dev

pkg-config --modversion libcurl
# 출력: 7.xx.x

dpkg -l | grep nlohmann-json
# 출력: nlohmann-json3-dev
```

---

## 빌드 테스트

```bash
cd VMG
mkdir build && cd build
cmake ..

# 예상 출력:
# -- Found OpenSSL: ...
# -- Found CURL: ...
# -- Found nlohmann_json: ...
# -- Found Paho MQTT C: ...
# -- Found Paho MQTT C++: ...
```

성공하면 다음 단계로:

```bash
make -j8
```

---

## 트러블슈팅

### 1. "libpaho-mqttpp-dev not found"

```bash
# Ubuntu 버전 확인
lsb_release -a

# Ubuntu 20.04 이하인 경우 소스에서 설치 필요:
git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp
mkdir build && cd build
cmake -DPAHO_BUILD_SHARED=TRUE -DPAHO_BUILD_STATIC=FALSE -DPAHO_WITH_SSL=TRUE ..
sudo make install
```

### 2. "Could not find CURL"

```bash
sudo apt install libcurl4-openssl-dev
```

### 3. "nlohmann_json not found"

```bash
# 수동 헤더 다운로드
sudo mkdir -p /usr/include/nlohmann
sudo wget -O /usr/include/nlohmann/json.hpp \
    https://github.com/nlohmann/json/releases/latest/download/json.hpp
```

---

## macOS (참고용)

```bash
brew install cmake openssl curl nlohmann-json mosquitto
brew install paho-mqtt-cpp
```

