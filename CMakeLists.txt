cmake_minimum_required(VERSION 3.14)
project(sim_robot_mqtt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(paho_mqtt_cpp
  GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp.git
  GIT_TAG v1.3.1
)
FetchContent_MakeAvailable(paho_mqtt_cpp)

add_executable(sim_robot src/main.cpp)

target_link_libraries(sim_robot PRIVATE paho-mqttpp3)
