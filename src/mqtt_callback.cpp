#include "mqtt_callback.h"

#include <glog/logging.h>

void MqttCallback::connection_lost(const std::string& cause) {
  LOG(WARNING) << "Connection lost: " << cause;
}

void MqttCallback::message_arrived(mqtt::const_message_ptr msg) {
  LOG(INFO) << "[收到订阅消息]";
  LOG(INFO) << "  主题: " << msg->get_topic();
  LOG(INFO) << "  内容: " << msg->to_string();
  LOG(INFO) << "  QoS: " << msg->get_qos();
}

void MqttCallback::delivery_complete(mqtt::delivery_token_ptr token) {}
