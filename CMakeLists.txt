cmake_minimum_required(VERSION 3.14)
project(sim_robot_mqtt VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 定义版本宏
add_definitions(-DPROJECT_VERSION="${PROJECT_VERSION}")

# 添加include目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# 查找系统安装的 Paho MQTT C++ 库
find_package(PahoMqttCpp REQUIRED)

# 查找 SQLite3 库
find_package(SQLite3 REQUIRED)

# 查找 glog 库
find_package(glog REQUIRED)

# 查找 nlohmann_json 库
find_package(nlohmann_json 3.2.0 REQUIRED)

add_executable(robot
    src/main.cpp
    src/config_db.cpp
    src/mqtt_manager.cpp
    src/robot.cpp
    src/protocol.cpp
    src/http_server.cpp
)

target_link_libraries(robot PRIVATE
    PahoMqttCpp::paho-mqttpp3
    SQLite::SQLite3
    glog::glog
    nlohmann_json::nlohmann_json
)
