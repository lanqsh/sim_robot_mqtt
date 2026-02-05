#include "robot.h"

#include <glog/logging.h>
#include <nlohmann/json.hpp>

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

void Robot::SetReportInterval(int interval_seconds) {
  report_interval_seconds_ = interval_seconds;
  LOG(INFO) << "[Robot " << robot_id_ << "] 设置上报间隔为 " << interval_seconds << " 秒";
}

void Robot::ReportThreadFunc() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 上报线程已启动，间隔: " << report_interval_seconds_ << "秒";

  while (!stop_report_.load()) {
    // 等待配置的间隔时间
    for (int i = 0; i < report_interval_seconds_ * 10 && !stop_report_.load(); ++i) {
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

std::string Robot::GetLastData() const {
  nlohmann::json j;

  // 基本信息
  j["robot_id"] = robot_id_;
  j["publish_topic"] = publish_topic_;
  j["subscribe_topic"] = subscribe_topic_;
  j["sequence"] = sequence_.load();
  j["report_interval_seconds"] = report_interval_seconds_;
  j["running"] = IsRunning();

  // RobotData 全量序列化
  nlohmann::json d;
  d["main_motor_current"] = data_.main_motor_current;
  d["slave_motor_current"] = data_.slave_motor_current;
  d["battery_voltage"] = data_.battery_voltage;
  d["battery_current"] = data_.battery_current;
  d["battery_status"] = data_.battery_status;
  d["battery_level"] = data_.battery_level;
  d["battery_temperature"] = data_.battery_temperature;
  d["position_info"] = data_.position_info;
  d["working_duration"] = data_.working_duration;
  d["total_run_count"] = data_.total_run_count;
  d["current_lap_count"] = data_.current_lap_count;
  d["solar_voltage"] = data_.solar_voltage;
  d["solar_current"] = data_.solar_current;

  // timestamp
  d["current_timestamp"] = {
    {"hour", data_.current_timestamp.hour},
    {"minute", data_.current_timestamp.minute},
    {"second", data_.current_timestamp.second}
  };

  d["board_temperature"] = data_.board_temperature;

  // 配置参数
  d["lora_params"] = {
    {"power", data_.lora_params.power},
    {"frequency", data_.lora_params.frequency},
    {"rate", data_.lora_params.rate}
  };

  d["robot_number"] = data_.robot_number;
  d["software_version"] = data_.software_version;
  d["parking_position"] = data_.parking_position;
  d["daytime_scan_protect"] = data_.daytime_scan_protect;

  // schedule tasks
  nlohmann::json tasks = nlohmann::json::array();
  for (const auto& t : data_.schedule_tasks) {
    tasks.push_back({
      {"weekday", t.weekday}, {"hour", t.hour}, {"minute", t.minute}, {"run_count", t.run_count}
    });
  }
  d["schedule_tasks"] = tasks;

  d["enabled"] = data_.enabled;

  // 电机参数
  d["motor_params"] = {
    {"walk_motor_speed", data_.motor_params.walk_motor_speed},
    {"brush_motor_speed", data_.motor_params.brush_motor_speed},
    {"windproof_motor_speed", data_.motor_params.windproof_motor_speed},
    {"walk_motor_max_current_ma", data_.motor_params.walk_motor_max_current_ma},
    {"brush_motor_max_current_ma", data_.motor_params.brush_motor_max_current_ma},
    {"windproof_motor_max_current_ma", data_.motor_params.windproof_motor_max_current_ma},
    {"walk_motor_warning_current_ma", data_.motor_params.walk_motor_warning_current_ma},
    {"brush_motor_warning_current_ma", data_.motor_params.brush_motor_warning_current_ma},
    {"windproof_motor_warning_current_ma", data_.motor_params.windproof_motor_warning_current_ma},
    {"walk_motor_mileage_m", data_.motor_params.walk_motor_mileage_m},
    {"brush_motor_timeout_s", data_.motor_params.brush_motor_timeout_s},
    {"windproof_motor_timeout_s", data_.motor_params.windproof_motor_timeout_s},
    {"reverse_time_s", data_.motor_params.reverse_time_s},
    {"protection_angle", data_.motor_params.protection_angle}
  };

  // 保护参数
  d["temp_voltage_protection"] = {
    {"protection_current_ma", data_.temp_voltage_protection.protection_current_ma},
    {"high_temp_threshold", data_.temp_voltage_protection.high_temp_threshold},
    {"low_temp_threshold", data_.temp_voltage_protection.low_temp_threshold},
    {"protection_temp", data_.temp_voltage_protection.protection_temp},
    {"recovery_temp", data_.temp_voltage_protection.recovery_temp},
    {"protection_voltage", data_.temp_voltage_protection.protection_voltage},
    {"recovery_voltage", data_.temp_voltage_protection.recovery_voltage},
    {"protection_battery_level", data_.temp_voltage_protection.protection_battery_level},
    {"limit_run_battery_level", data_.temp_voltage_protection.limit_run_battery_level},
    {"recovery_battery_level", data_.temp_voltage_protection.recovery_battery_level},
    {"board_protection_temp", data_.temp_voltage_protection.board_protection_temp},
    {"board_recovery_temp", data_.temp_voltage_protection.board_recovery_temp}
  };

  // 本地时间
  d["local_time"] = {
    {"year", data_.local_time.year}, {"month", data_.local_time.month}, {"day", data_.local_time.day},
    {"hour", data_.local_time.hour}, {"minute", data_.local_time.minute}, {"second", data_.local_time.second},
    {"weekday", data_.local_time.weekday}
  };

  // 环境信息
  d["environment_info"] = {
    {"sensor_temperature", data_.environment_info.sensor_temperature},
    {"sensor_humidity", data_.environment_info.sensor_humidity},
    {"ambient_temperature", data_.environment_info.ambient_temperature},
    {"day_night_status", data_.environment_info.day_night_status}
  };

  // 数组数据
  d["master_currents"] = data_.master_currents;
  d["slave_currents"] = data_.slave_currents;
  d["position"] = data_.position;
  d["direction"] = data_.direction;

  // 设备标识
  d["module_eui"] = data_.module_eui;
  d["domestic_foreign_flag"] = data_.domestic_foreign_flag;
  d["country_code"] = data_.country_code;
  d["region_code"] = data_.region_code;
  d["project_code"] = data_.project_code;

  j["data"] = d;

  return j.dump();
}
