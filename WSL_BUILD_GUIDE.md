# VMG WSL 빌드 가이드

Windows에서는 의존성 설치가 복잡하므로 **WSL(Ubuntu)에서 빌드**를 권장합니다.

## 🐧 WSL 설치 (Windows 10/11)

```powershell
# PowerShell (관리자 권한)
wsl --install
```

재부팅 후:

```powershell
# Ubuntu 설치 확인
wsl --list --verbose

# WSL 진입
wsl
```

---

## 📦 의존성 설치 (WSL Ubuntu 내부)

```bash
# 시스템 업데이트
sudo apt update && sudo apt upgrade -y

# 빌드 도구
sudo apt install -y build-essential cmake git

# 필수 라이브러리
sudo apt install -y \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    zlib1g-dev

# 확인
cmake --version
g++ --version
python3 --version
```

---

## 🚀 빌드 및 테스트 (한 방에!)

```bash
# VMG 프로젝트 디렉토리로 이동
cd /mnt/c/Users/user/AURIX-v1.10.24-workspace/OTA-Zonal_Gateway/VMG

# 테스트 스크립트 실행 권한 부여
chmod +x test/build_and_test.sh

# 빌드 + 테스트 실행
cd test
./build_and_test.sh
```

---

## 📊 예상 출력

```
============================================================
VMG Build and Test Script
============================================================
→ Checking dependencies...
✓ cmake found
✓ make found
✓ g++ found
✓ python3 found

============================================================
Running CMake
============================================================
-- The CXX compiler identification is GNU 11.4.0
-- Found OpenSSL: 3.0.2
-- Found CURL: 7.81.0
-- Found nlohmann_json: 3.11.2
-- Found Paho MQTT C: /usr/lib/x86_64-linux-gnu/libpaho-mqtt3as.so
-- Found Paho MQTT C++: /usr/lib/x86_64-linux-gnu/libpaho-mqttpp3.so
-- Configuring done
-- Generating done
✓ CMake completed

============================================================
Building VMG
============================================================
[  5%] Building CXX object CMakeFiles/vmg.dir/main.cpp.o
[ 10%] Building CXX object CMakeFiles/vmg.dir/src/app/config_manager.cpp.o
...
[100%] Linking CXX executable vmg
✓ Build completed
✓ VMG executable created: build/vmg

============================================================
Running Tests
============================================================
→ Test 1: Configuration validation...
✓ All tests passed!

→ Test 2: Package parser test...
✓ All tests passed!

============================================================
Build and Test Summary
============================================================
✓ Build: SUCCESS
✓ Tests: PASSED

→ VMG executable: build/vmg
→ Run with: cd build && ./vmg ../config.json
```

---

## 🔧 트러블슈팅

### 1. WSL 경로 문제
Windows 경로는 `/mnt/c/...` 형식으로 접근:
```bash
# Windows: C:\Users\user\project
# WSL:     /mnt/c/Users/user/project
```

### 2. 권한 문제
```bash
# 실행 권한 부여
chmod +x test/build_and_test.sh

# 또는 직접 실행
bash test/build_and_test.sh
```

### 3. 의존성 누락
```bash
# 패키지 검색
apt-cache search paho-mqtt

# 특정 패키지 설치
sudo apt install libpaho-mqttpp-dev
```

### 4. CMake 버전 낮음
```bash
# CMake 3.20+ 설치 (필요시)
sudo apt remove cmake
sudo snap install cmake --classic
```

---

## 🎯 빠른 시작 (복붙용)

```bash
# WSL 진입 (PowerShell에서)
wsl

# 의존성 설치 (WSL Ubuntu 내부)
sudo apt update
sudo apt install -y build-essential cmake git libssl-dev \
    libcurl4-openssl-dev nlohmann-json3-dev \
    libpaho-mqtt-dev libpaho-mqttpp-dev zlib1g-dev

# 프로젝트로 이동
cd /mnt/c/Users/user/AURIX-v1.10.24-workspace/OTA-Zonal_Gateway/VMG

# 빌드 + 테스트
chmod +x test/build_and_test.sh
cd test
./build_and_test.sh
```

---

## 💡 WSL vs Windows Native

| 항목 | WSL (Ubuntu) | Windows Native |
|------|--------------|----------------|
| 의존성 설치 | ✅ 쉬움 (apt) | ❌ 어려움 (수동) |
| 빌드 속도 | ✅ 빠름 | ⚠️ 느림 |
| 라이브러리 호환성 | ✅ 완벽 | ❌ 제한적 |
| 실제 배포 환경 | ✅ 동일 (Linux) | ❌ 다름 |
| 권장도 | ⭐⭐⭐⭐⭐ | ⭐⭐ |

**결론: WSL 사용을 강력 추천!** 🐧

---

## 🚀 실행 방법

빌드 후 VMG 실행:

```bash
# WSL 내부
cd /mnt/c/Users/user/AURIX-v1.10.24-workspace/OTA-Zonal_Gateway/VMG/build
./vmg ../config.json
```

---

## 📝 참고

- WSL 파일 시스템: `/mnt/c/` = `C:\`
- WSL과 Windows 간 파일 공유 가능
- VS Code WSL 확장으로 편리한 개발 가능

