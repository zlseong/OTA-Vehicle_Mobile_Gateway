cmake_minimum_required(VERSION 3.10)
project(vehicle_gateway VERSION 2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Source files
add_executable(gateway
    main.cpp
    http_client.cpp
)

# Link libraries
target_link_libraries(gateway
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Installation
install(TARGETS gateway DESTINATION bin)
install(FILES config.json DESTINATION etc/vehicle_gateway)

