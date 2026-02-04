#include "robot.h"

#include <glog/logging.h>

#include <fstream>
#include <sstream>

#include "mqtt_manager.h"

// 上行数据模板占位符
#define PLACEHOLDER_DEV_EUI "{{DEV_EUI}}"
#define PLACEHOLDER_DEV_ADDR "{{DEV_ADDR}}"
#define PLACEHOLDER_DATA "{{DATA}}"

// 上行数据模板文件路径
#define UPLINK_TEMPLATE_FILE "uplink_template.json"

// 静态成员初始化
std::string Robot::uplink_template_ = "";

Robot::Robot(const std::string& robot_id) : robot_id_(robot_id), sequence_(0) {
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

  // 首次加载模板
  if (uplink_template_.empty()) {
    uplink_template_ = LoadUplinkTemplate();
  }
}

Robot::~Robot() {
  // 停止上报线程
  stop_report_.store(true);
  if (report_thread_.joinable()) {
    report_thread_.join();
  }
}

void Robot::SetTopics(const std::string& publish_topic,
                      const std::string& subscribe_topic) {
  publish_topic_ = publish_topic;
  subscribe_topic_ = subscribe_topic;
}

std::string Robot::GenerateUplinkPayload(const std::string& data) {
  if (uplink_template_.empty()) {
    LOG(ERROR) << "上行数据模板为空";
    return "";
  }

  // 获取devAddr（机器人ID的后8个字符）
  std::string dev_addr = robot_id_.length() >= 8
                             ? robot_id_.substr(robot_id_.length() - 8)
                             : robot_id_;

  // 替换模板中的占位符
  std::string result = uplink_template_;

  // 替换devEui
  const std::string placeholder_dev_eui = PLACEHOLDER_DEV_EUI;
  size_t pos = result.find(placeholder_dev_eui);
  if (pos != std::string::npos) {
    result.replace(pos, placeholder_dev_eui.length(), robot_id_);
  }

  // 替换devAddr
  const std::string placeholder_dev_addr = PLACEHOLDER_DEV_ADDR;
  pos = result.find(placeholder_dev_addr);
  if (pos != std::string::npos) {
    result.replace(pos, placeholder_dev_addr.length(), dev_addr);
  }

  // 替换data
  const std::string placeholder_data = PLACEHOLDER_DATA;
  pos = result.find(placeholder_data);
  if (pos != std::string::npos) {
    result.replace(pos, placeholder_data.length(), data);
  }

  return result;
}

std::string Robot::LoadUplinkTemplate() {
  std::ifstream file(UPLINK_TEMPLATE_FILE);
  if (!file.is_open()) {
    LOG(ERROR) << "无法打开上行数据模板文件: " << UPLINK_TEMPLATE_FILE;
    return "";
  }

  std::ostringstream oss;
  oss << file.rdbuf();
  file.close();

  LOG(INFO) << "成功加载上行数据模板";
  return oss.str();
}

void Robot::HandleMessage(const std::string& data) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 收到消息";
  LOG(INFO) << "  内容: " << data;
}

void Robot::SetMqttManager(std::shared_ptr<MqttManager> manager) {
  mqtt_manager_ = manager;

  // 启动上报线程
  if (manager != nullptr) {
    stop_report_.store(false);
    report_thread_ = std::thread(&Robot::ReportThreadFunc, this);
  }
}

void Robot::ReportThreadFunc() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 上报线程已启动";

  while (!stop_report_.load()) {
    // 等待10秒
    for (int i = 0; i < 100 && !stop_report_.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (stop_report_.load()) break;

    // 生成并发送上报数据
    auto mqtt_manager = mqtt_manager_.lock();
    if (mqtt_manager) {
      // TODO: 生成实际的通信数据
      std::string data = "aIIACwAB8ugW";  // 示例数据
      std::string payload = GenerateUplinkPayload(data);

      // 将消息加入发送队列
      mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);
      LOG(INFO) << "[Robot " << robot_id_ << "] 上报数据已加入队列";
    }
  }

  LOG(INFO) << "[Robot " << robot_id_ << "] 上报线程已停止";
}
