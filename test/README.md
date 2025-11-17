# VMG Test Suite

자동 빌드 및 테스트 스크립트

## 📋 테스트 목록

### 1. **Configuration Test** (`test_config.py`)
- config.json 유효성 검증
- 3-Sector Layout 설정 확인
- Vehicle 정보 검증
- MQTT Topic 설정 확인

### 2. **Package Parser Test** (`test_package_parser.py`)
- Vehicle Package 생성 테스트
- Vehicle Package 구조 검증
- Zone Package 추출 테스트
- Magic Number 검증
- CRC32 검증

### 3. **Integration Test** (선택)
- 전체 OTA 플로우 시뮬레이션
- VMG ↔ Server 통신 테스트

## 🚀 사용 방법

### Linux/macOS

```bash
# 실행 권한 부여
chmod +x test/build_and_test.sh

# 빌드 + 테스트 실행
cd test
./build_and_test.sh

# 클린 빌드
./build_and_test.sh --clean
```

### Windows

```cmd
# 빌드 + 테스트 실행
cd test
build_and_test.bat

# 클린 빌드
build_and_test.bat --clean
```

### 개별 테스트 실행

```bash
# Configuration test만 실행
python test/test_config.py

# Package parser test만 실행
python test/test_package_parser.py
```

## 📊 테스트 결과 예제

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

============================================================
Test 1: config.json Validity
============================================================
✓ config.json is valid JSON
✓ server: OK
✓ vehicle: OK
✓ device: OK
✓ ota: OK
✓ Test 1 PASSED

============================================================
Test 2: 3-Sector Layout Configuration
============================================================
Partition A: /dev/mmcblk0p2
Partition B: /dev/mmcblk0p3
Data Partition: /dev/mmcblk0p4
Data Mount: /mnt/data
✓ 3-Sector layout properly configured
✓ All paths use data partition (/mnt/data)
✓ Test 2 PASSED

============================================================
Test Summary
============================================================
Tests Passed: 4
Tests Failed: 0
✓ All tests passed!
```

## 🔧 CI/CD 통합

### GitHub Actions 예제

```yaml
name: VMG Build and Test

on: [push, pull_request]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake g++ libssl-dev libcurl4-openssl-dev \
                                  nlohmann-json3-dev libpaho-mqtt-dev libpaho-mqttpp-dev
      
      - name: Run tests
        run: |
          cd test
          ./build_and_test.sh
```

## 📝 테스트 추가 방법

새로운 테스트를 추가하려면:

1. `test/` 디렉토리에 `test_xxx.py` 파일 생성
2. `build_and_test.sh`에 테스트 호출 추가
3. 테스트 통과 시 `exit 0`, 실패 시 `exit 1` 반환

## 🐛 트러블슈팅

### CMake Error
```bash
# 의존성 재설치
sudo apt-get install -y cmake libssl-dev libcurl4-openssl-dev
```

### Python Import Error
```bash
# Python 경로 확인
export PYTHONPATH=$PYTHONPATH:$(pwd)/tools
```

### 빌드 실패
```bash
# 클린 빌드
./build_and_test.sh --clean
```

## ✅ 체크리스트

빌드 및 테스트 전 확인사항:

- [ ] CMake 3.10 이상 설치
- [ ] GCC/Clang 컴파일러 설치
- [ ] Python 3.6 이상 설치
- [ ] 필수 라이브러리 설치 (OpenSSL, libcurl, nlohmann_json, Paho MQTT)
- [ ] config.json이 프로젝트 루트에 존재
- [ ] 실행 권한 설정 (`chmod +x test/build_and_test.sh`)

