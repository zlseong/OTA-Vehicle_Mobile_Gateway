#!/bin/bash
#
# VMG Build and Test Script
# 
# 자동으로 빌드하고 모든 테스트를 실행합니다.
#

set -e  # Exit on error

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_header() {
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}============================================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}→ $1${NC}"
}

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

print_header "VMG Build and Test Script"

# Step 1: Check dependencies
print_info "Checking dependencies..."

check_command() {
    if command -v $1 &> /dev/null; then
        print_success "$1 found"
        return 0
    else
        print_error "$1 not found"
        return 1
    fi
}

DEPS_OK=true
check_command cmake || DEPS_OK=false
check_command make || DEPS_OK=false
check_command g++ || DEPS_OK=false
check_command python3 || DEPS_OK=false

if [ "$DEPS_OK" = false ]; then
    print_error "Missing dependencies. Please install required tools."
    exit 1
fi

# Step 2: Clean build directory
if [ "$1" == "--clean" ]; then
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    print_success "Build directory cleaned"
fi

# Step 3: Create build directory
print_info "Creating build directory..."
mkdir -p "$BUILD_DIR"

# Step 4: Run CMake
print_header "Running CMake"
cd "$BUILD_DIR"
cmake .. || {
    print_error "CMake failed"
    exit 1
}
print_success "CMake completed"

# Step 5: Build
print_header "Building VMG"
make -j$(nproc) || {
    print_error "Build failed"
    exit 1
}
print_success "Build completed"

# Check if executable exists
if [ -f "$BUILD_DIR/vmg" ]; then
    print_success "VMG executable created: $BUILD_DIR/vmg"
    ls -lh "$BUILD_DIR/vmg"
else
    print_error "VMG executable not found"
    exit 1
fi

# Step 6: Run tests
print_header "Running Tests"

cd "$SCRIPT_DIR"

# Test 1: Configuration test
print_info "Test 1: Configuration validation..."
python3 test_config.py || {
    print_error "Configuration test failed"
    exit 1
}

# Test 2: Package parser test
print_info "Test 2: Package parser test..."
python3 test_package_parser.py || {
    print_error "Package parser test failed"
    exit 1
}

# Test 3: Integration test (if enabled)
if [ -f "test_integration.py" ]; then
    print_info "Test 3: Integration test..."
    python3 test_integration.py || {
        print_warning "Integration test failed (optional)"
    }
fi

# Summary
print_header "Build and Test Summary"
print_success "Build: SUCCESS"
print_success "Tests: PASSED"
echo ""
print_info "VMG executable: $BUILD_DIR/vmg"
print_info "Run with: cd build && ./vmg ../config.json"
echo ""

exit 0

