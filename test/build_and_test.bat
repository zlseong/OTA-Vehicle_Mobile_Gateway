@echo off
REM VMG Build and Test Script (Windows)
REM 
REM 자동으로 빌드하고 모든 테스트를 실행합니다.
REM

setlocal enabledelayedexpansion

echo ============================================================
echo VMG Build and Test Script (Windows)
echo ============================================================
echo.

REM Get script directory
set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..
set BUILD_DIR=%PROJECT_DIR%\build

REM Step 1: Check dependencies
echo [INFO] Checking dependencies...

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] cmake not found
    exit /b 1
)
echo [OK] cmake found

where python >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] python not found
    exit /b 1
)
echo [OK] python found

REM Step 2: Clean build directory (if --clean)
if "%1"=="--clean" (
    echo [INFO] Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    echo [OK] Build directory cleaned
)

REM Step 3: Create build directory
echo [INFO] Creating build directory...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Step 4: Run CMake
echo.
echo ============================================================
echo Running CMake
echo ============================================================
cd /d "%BUILD_DIR%"
cmake .. -G "MinGW Makefiles"
if %errorlevel% neq 0 (
    echo [ERROR] CMake failed
    exit /b 1
)
echo [OK] CMake completed

REM Step 5: Build
echo.
echo ============================================================
echo Building VMG
echo ============================================================
cmake --build . -- -j4
if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    exit /b 1
)
echo [OK] Build completed

REM Check if executable exists
if exist "%BUILD_DIR%\vmg.exe" (
    echo [OK] VMG executable created: %BUILD_DIR%\vmg.exe
) else (
    echo [ERROR] VMG executable not found
    exit /b 1
)

REM Step 6: Run tests
echo.
echo ============================================================
echo Running Tests
echo ============================================================

cd /d "%SCRIPT_DIR%"

REM Test 1: Configuration test
echo [INFO] Test 1: Configuration validation...
python test_config.py
if %errorlevel% neq 0 (
    echo [ERROR] Configuration test failed
    exit /b 1
)

REM Test 2: Package parser test
echo [INFO] Test 2: Package parser test...
python test_package_parser.py
if %errorlevel% neq 0 (
    echo [ERROR] Package parser test failed
    exit /b 1
)

REM Summary
echo.
echo ============================================================
echo Build and Test Summary
echo ============================================================
echo [OK] Build: SUCCESS
echo [OK] Tests: PASSED
echo.
echo [INFO] VMG executable: %BUILD_DIR%\vmg.exe
echo [INFO] Run with: cd build ^&^& vmg.exe ..\config.json
echo.

exit /b 0

