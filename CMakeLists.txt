# CMakeLists.txt for Vehicle Mobile Gateway (VMG)
cmake_minimum_required(VERSION 3.10)
project(VMG VERSION 2.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Find required packages
find_package(OpenSSL 3.0 REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(CURL REQUIRED)

# Paho MQTT C++
find_library(PAHO_MQTT_C paho-mqtt3as REQUIRED)
find_library(PAHO_MQTT_CPP paho-mqttpp3 REQUIRED)

# Source files
set(SOURCES
    main.cpp
    
    # App
    src/app/config_manager.cpp
    src/app/vehicle_state.cpp
    src/app/system_manager.cpp
    
    # VCI
    src/vci/vci_collector.cpp
    
    # Readiness
    src/readiness/readiness_manager.cpp
    
    # HTTP Client
    src/http/http_client.cpp
    
    # MQTT Client
    src/mqtt/mqtt_client.cpp
    
    # DoIP Client (parallel with ZGW)
    src/doip/doip_client.cpp
    
    # OTA Management (parallel with ZGW FlashBankManager)
    src/ota/partition_manager.cpp
    src/ota/ota_manager.cpp
    src/ota/ota_manager_vehicle.cpp
    
    # Package Parsers (3-layer hierarchy)
    src/package/vehicle_package_parser.cpp
    src/package/zone_package_parser.cpp
)

# PQC TLS Client (if available)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/pqc_tls/pqc_tls_client.c")
    list(APPEND SOURCES src/pqc_tls/pqc_tls_client.c)
endif()

# Executable
add_executable(vmg ${SOURCES})

# Link libraries
target_link_libraries(vmg
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    nlohmann_json::nlohmann_json
    ${PAHO_MQTT_C}
    ${PAHO_MQTT_CPP}
    pthread
    z  # zlib for CRC32
)

# Installation
install(TARGETS vmg DESTINATION bin)
install(FILES config.json DESTINATION etc/vmg)

# Print configuration summary
message(STATUS "==========================================")
message(STATUS "VMG Build Configuration")
message(STATUS "==========================================")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
message(STATUS "==========================================")
