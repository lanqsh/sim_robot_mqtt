#include "robot.h"

#include <glog/logging.h>
#include <sstream>

Robot::Robot(const std::string& robot_id)
    : robot_id_(robot_id), sequence_(0), battery_level_(100) {}

Robot::~Robot() {}

void Robot::SetTopics(const std::string& publish_topic,
                      const std::string& subscribe_topic) {
  publish_topic_ = publish_topic;
  subscribe_topic_ = subscribe_topic;
}

std::string Robot::GeneratePayload() {
  int seq = sequence_.fetch_add(1);
  battery_level_ = 100 - (seq % 100);

  std::ostringstream oss;
  oss << "{\"seq\": " << seq << ", \"battery\": " << battery_level_
      << ", \"robot_id\": \"" << robot_id_ << "\"}";
  return oss.str();
}

void Robot::HandleMessage(const std::string& topic, const std::string& payload) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 收到消息";
  LOG(INFO) << "  主题: " << topic;
  LOG(INFO) << "  内容: " << payload;
}
