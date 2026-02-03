cmake_minimum_required(VERSION 3.14)
project(sim_robot_mqtt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找系统安装的 Paho MQTT C++ 库
find_package(PahoMqttCpp REQUIRED)

# 查找 SQLite3 库
find_package(SQLite3 REQUIRED)

# 查找 glog 库
find_package(glog REQUIRED)

add_executable(sim_robot
    src/main.cpp
    src/config_db.cpp
    src/mqtt_callback.cpp
)

target_link_libraries(sim_robot PRIVATE
    PahoMqttCpp::paho-mqttpp3
    SQLite::SQLite3
    glog::glog
)
