cmake_minimum_required(VERSION 3.10)
project(vehicle_gateway VERSION 2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# Source files
add_executable(gateway
    main.cpp
    http_client.cpp
    mqtt_client.cpp
)

# Include directories
target_include_directories(gateway PRIVATE ${MOSQUITTO_INCLUDE_DIRS})

# Link libraries
target_link_libraries(gateway
    CURL::libcurl
    nlohmann_json::nlohmann_json
    ${MOSQUITTO_LIBRARIES}
)

# Link directories
target_link_directories(gateway PRIVATE ${MOSQUITTO_LIBRARY_DIRS})

# Compile flags
target_compile_options(gateway PRIVATE ${MOSQUITTO_CFLAGS_OTHER})

# Installation
install(TARGETS gateway DESTINATION bin)
install(FILES config.json DESTINATION etc/vehicle_gateway)

