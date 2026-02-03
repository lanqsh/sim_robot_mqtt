cmake_minimum_required(VERSION 3.14)
project(sim_robot_mqtt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 定义版本宏
add_definitions(-DPROJECT_VERSION="${PROJECT_VERSION}")

# 查找系统安装的 Paho MQTT C++ 库
find_package(PahoMqttCpp REQUIRED)

# 查找 SQLite3 库
find_package(SQLite3 REQUIRED)

# 查找 glog 库
find_package(glog REQUIRED)

add_executable(robot
    src/main.cpp
    src/config_db.cpp
    src/mqtt_callback.cpp
)

target_link_libraries(robot PRIVATE
    PahoMqttCpp::paho-mqttpp3
    SQLite::SQLite3
    glog::glog
)
