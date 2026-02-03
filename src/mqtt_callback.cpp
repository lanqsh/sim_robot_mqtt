#include "mqtt_callback.h"

#include <iostream>

void MqttCallback::connection_lost(const std::string& cause) {
  std::cout << "Connection lost: " << cause << std::endl;
}

void MqttCallback::message_arrived(mqtt::const_message_ptr msg) {
  std::cout << "\n[收到订阅消息]" << std::endl;
  std::cout << "  主题: " << msg->get_topic() << std::endl;
  std::cout << "  内容: " << msg->to_string() << std::endl;
  std::cout << "  QoS: " << msg->get_qos() << std::endl;
}

void MqttCallback::delivery_complete(mqtt::delivery_token_ptr token) {}
