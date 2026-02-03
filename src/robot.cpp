#include "robot.h"

#include <glog/logging.h>
#include <sstream>

Robot::Robot(const std::string& robot_id)
    : robot_id_(robot_id), sequence_(0) {
  // 初始化机器人数据
  data_.battery_level = 100;
  data_.battery_voltage = 0;
  data_.battery_current = 0;
  data_.battery_status = 0;
  data_.battery_temperature = 0;
  data_.main_motor_current = 0;
  data_.slave_motor_current = 0;
  data_.working_duration = 0;
  data_.total_run_count = 0;
  data_.current_lap_count = 0;
  data_.solar_voltage = 0;
  data_.solar_current = 0;
  data_.board_temperature = 0;
  data_.parking_position = 0;
  data_.daytime_scan_protect = false;
  data_.enabled = true;
  data_.position = 0;
  data_.direction = 0;
  data_.domestic_foreign_flag = 0;
}

Robot::~Robot() {}

void Robot::SetTopics(const std::string& publish_topic,
                      const std::string& subscribe_topic) {
  publish_topic_ = publish_topic;
  subscribe_topic_ = subscribe_topic;
}

std::string Robot::GeneratePayload() {
  int seq = sequence_.fetch_add(1);

  std::ostringstream oss;
  oss << "{\"seq\": " << seq << ", \"robot_id\": \"" << robot_id_ << "\"}";
  return oss.str();
}

void Robot::HandleMessage(const std::string& topic, const std::string& payload) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 收到消息";
  LOG(INFO) << "  主题: " << topic;
  LOG(INFO) << "  内容: " << payload;
}
