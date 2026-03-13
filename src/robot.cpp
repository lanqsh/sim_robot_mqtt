#include "robot.h"

#include <glog/logging.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "config_db.h"
#include "mqtt_manager.h"

// 上行数据模板占位符
#define PLACEHOLDER_DEV_EUI "{{DEV_EUI}}"
#define PLACEHOLDER_DEV_ADDR "{{DEV_ADDR}}"
#define PLACEHOLDER_DATA "{{DATA}}"

// 上行数据模板文件路径
#define UPLINK_TEMPLATE_FILE "doc/uplink_template.json"

// 静态成员初始化
std::string Robot::uplink_template_ = "";

static bool IsWindProtectionEnabled(uint8_t protection_info) {
  return (protection_info & 0x80) != 0;
}

static bool IsHumidityProtectionEnabled(uint8_t protection_info) {
  return (protection_info & 0x40) != 0;
}

static bool IsBracketProtectionEnabled(uint8_t protection_info) {
  return (protection_info & 0x20) != 0;
}

static bool IsAmbientTemperatureProtectionEnabled(uint8_t protection_info) {
  return (protection_info & 0x10) != 0;
}

Robot::Robot(const std::string& robot_id, uint16_t robot_number) : robot_id_(robot_id), sequence_(0) {
  // 初始化机器人数据
  data_.alarm_fa = 0;
  data_.alarm_fb = 0;
  data_.alarm_fc = 0;
  data_.alarm_fd = 0;
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

  // 初始化定时任务，保留7个元素
  data_.schedule_tasks.resize(7);
  for (auto& task : data_.schedule_tasks) {
    task.weekday = 0;
    task.hour = 0;
    task.minute = 0;
    task.run_count = 0;
  }

  // 首次加载模板
  if (uplink_template_.empty()) {
    uplink_template_ = LoadUplinkTemplate();
  }

  // 初始化清扫记录，保留5条
  data_.clean_records.resize(5);

  // 初始化主/从机电流，保留16个元素
  data_.master_currents.resize(16);
  data_.slave_currents.resize(16);

  // 记录创建时间（用于计算工作时长）
  creation_time_ = std::chrono::system_clock::now();

  // 设置机器人序号（数字）
  data_.robot_number = std::to_string(robot_number);

  // 将软件版本设为 CMake 中定义的 PROJECT_VERSION（如果存在）
#ifdef PROJECT_VERSION
  data_.software_version = PROJECT_VERSION;
#else
  data_.software_version = "0.0";
#endif
}

Robot::~Robot() {
  // 停止上报线程
  StopReport();
}

uint64_t Robot::BeginRequestReplyTracking() {
  std::lock_guard<std::mutex> lock(request_reply_mutex_);
  ++request_reply_token_;
  if (request_reply_token_ == 0) {
    ++request_reply_token_;
  }
  data_.request_reply.available = false;
  return request_reply_token_;
}

bool Robot::GetRequestReplyStatus(uint64_t request_id,
                                  RobotData::RequestReply* reply,
                                  bool* received) const {
  if (reply == nullptr || received == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(request_reply_mutex_);
  *reply = data_.request_reply;

  if (request_id == 0) {
    *received = data_.request_reply.available;
    return true;
  }

  *received = (request_reply_completed_token_ >= request_id) && data_.request_reply.available;
  return true;
}

bool Robot::WaitForRequestReply(uint64_t request_id,
                                int timeout_ms,
                                RobotData::RequestReply* reply,
                                bool* received) const {
  if (reply == nullptr || received == nullptr) {
    return false;
  }

  if (timeout_ms < 0) {
    timeout_ms = 0;
  }

  std::unique_lock<std::mutex> lock(request_reply_mutex_);
  auto has_received = [this, request_id]() {
    if (request_id == 0) {
      return data_.request_reply.available;
    }
    return (request_reply_completed_token_ >= request_id) && data_.request_reply.available;
  };

  if (timeout_ms > 0 && !has_received()) {
    request_reply_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), has_received);
  }

  *reply = data_.request_reply;
  *received = has_received();
  return true;
}

void Robot::MarkRequestReplyReceived() {
  std::lock_guard<std::mutex> lock(request_reply_mutex_);
  request_reply_completed_token_ = request_reply_token_;
  request_reply_cv_.notify_all();
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

void Robot::SetMqttManager(std::shared_ptr<MqttManager> manager) {
  mqtt_manager_ = manager;
  LOG(INFO) << "[Robot " << robot_id_ << "] MQTT管理器已设置";

  // 自动启动定时上报
  StartReport();

  // 启动后立即发送一次Lora参数&清扫设置上报
  SendLoraAndCleanSettingsReport();
}

void Robot::SetConfigDb(std::shared_ptr<ConfigDb> config_db) {
  config_db_ = config_db;
  LOG(INFO) << "[Robot " << robot_id_ << "] ConfigDb已设置";
}

void Robot::UpdateAlarmsToDb() {
  auto config_db = config_db_.lock();
  if (!config_db) {
    LOG(WARNING) << "[Robot " << robot_id_ << "] ConfigDb不可用，无法更新告警到数据库";
    return;
  }

  ConfigDb::AlarmData alarms;
  alarms.alarm_fa = data_.alarm_fa;
  alarms.alarm_fb = data_.alarm_fb;
  alarms.alarm_fc = data_.alarm_fc;
  alarms.alarm_fd = data_.alarm_fd;

  if (config_db->UpdateRobotAlarms(robot_id_, alarms)) {
    LOG(INFO) << "[Robot " << robot_id_ << "] 告警已更新到数据库: "
              << "FA=0x" << std::hex << alarms.alarm_fa
              << ", FB=0x" << alarms.alarm_fb
              << ", FC=0x" << alarms.alarm_fc
              << ", FD=0x" << alarms.alarm_fd;

    if (!config_db->UpdateRobotDataSnapshot(robot_id_, SerializeDataSnapshot())) {
      LOG(WARNING) << "[Robot " << robot_id_ << "] 告警更新后写入快照失败";
    }
  } else {
    LOG(ERROR) << "[Robot " << robot_id_ << "] 告警更新到数据库失败";
  }
}

void Robot::HandleMessage(const std::string& data) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 收到消息";
  LOG(INFO) << "  Base64内容: " << data;

  try {
    // Base64解码
    std::vector<uint8_t> raw_bytes = Protocol::Base64ToBytes(data);
    LOG(INFO) << "  解码后字节: " << Protocol::BytesToHexString(raw_bytes);

    // 协议解码
    ProtocolFrame frame;
    if (protocol_.Decode(raw_bytes, frame)) {
      LOG(INFO) << "  协议解析成功:";
      LOG(INFO) << "    控制码: 0x" << std::hex << static_cast<int>(frame.control_code);
      LOG(INFO) << "    编号: 0x" << std::hex << frame.number;
      LOG(INFO) << "    帧计数: " << std::dec << static_cast<int>(frame.frame_count);
      LOG(INFO) << "    数据长度: " << static_cast<int>(frame.length);
      LOG(INFO) << "    数据域: " << Protocol::BytesToHexString(frame.data);

      // 根据数据域的标识字段处理不同类型的命令
      if (!frame.data.empty()) {
        uint8_t identifier = frame.data[0];  // 第一个字节是标识
        LOG(INFO) << "    标识符: 0x" << std::hex << static_cast<int>(identifier);

        // 根据标识符处理不同的命令
        switch (identifier) {
          case 0xC2: {  // 查询指令：本地时间&环境信息
            LOG(INFO) << "    命令类型: 查询本地时间&环境信息";

            if (frame.control_code != CONTROL_CODE_UPLINK) {
              LOG(INFO) << "    非平台下发控制码(0x"
                        << std::hex << static_cast<int>(frame.control_code)
                        << ")，忽略查询指令";
              break;
            }

            // 查询指令仅包含标识符
            if (frame.data.size() != 1) {
              LOG(ERROR) << "    查询指令数据长度错误, 期望1, 实际: "
                         << frame.data.size();
              break;
            }

            auto mqtt_manager = mqtt_manager_.lock();
            if (!mqtt_manager) {
              LOG(ERROR) << "    MQTT管理器未初始化，无法回复查询指令";
              break;
            }

            // 更新时间字段后再回复，确保返回当前本地时间
            UpdateTimeFields();

            // 构造响应数据域：标识(0xC2) + 本地时间(7字节) + 环境信息(7字节)
            // 环境信息按协议：温度/湿度/环境温度各2字节 + 白夜状态1字节
            std::vector<uint8_t> response_data;
            response_data.reserve(15);
            response_data.push_back(0xC2);

            int year = data_.local_time.year % 100;
            response_data.push_back(static_cast<uint8_t>(year & 0xFF));
            response_data.push_back(static_cast<uint8_t>(data_.local_time.month));
            response_data.push_back(static_cast<uint8_t>(data_.local_time.day));
            response_data.push_back(static_cast<uint8_t>(data_.local_time.hour));
            response_data.push_back(static_cast<uint8_t>(data_.local_time.minute));
            response_data.push_back(static_cast<uint8_t>(data_.local_time.second));
            response_data.push_back(static_cast<uint8_t>(data_.local_time.weekday));

            uint16_t sensor_temp =
                static_cast<uint16_t>(data_.environment_info.sensor_temperature);
            uint16_t sensor_humidity =
                static_cast<uint16_t>(data_.environment_info.sensor_humidity);
            uint16_t ambient_temp =
                static_cast<uint16_t>(data_.environment_info.ambient_temperature);

            response_data.push_back(static_cast<uint8_t>(sensor_temp >> 8));
            response_data.push_back(static_cast<uint8_t>(sensor_temp & 0xFF));
            response_data.push_back(static_cast<uint8_t>(sensor_humidity >> 8));
            response_data.push_back(static_cast<uint8_t>(sensor_humidity & 0xFF));
            response_data.push_back(static_cast<uint8_t>(ambient_temp >> 8));
            response_data.push_back(static_cast<uint8_t>(ambient_temp & 0xFF));
            response_data.push_back(
                static_cast<uint8_t>(data_.environment_info.day_night_status));

            // 回复沿用请求中的编号与帧计数
            std::vector<uint8_t> encoded =
                protocol_.Encode(CONTROL_CODE_DOWNLINK, frame.number,
                                 frame.frame_count, response_data);
            std::string base64_data = Protocol::BytesToBase64(encoded);
            std::string payload = GenerateUplinkPayload(base64_data);
            mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

            LOG(INFO) << "    查询回复已发送: "
                      << Protocol::BytesToHexString(encoded);
            break;
          }

          case 0xC1: {  // 查询指令：电机参数&温度电压参数
          LOG(INFO) << "    命令类型: 查询电机参数&温度电压参数";

          if (frame.control_code != CONTROL_CODE_UPLINK) {
            LOG(INFO) << "    非平台下发控制码(0x"
                << std::hex << static_cast<int>(frame.control_code)
                << ")，忽略查询指令";
            break;
          }

          // 查询指令仅包含标识符
          if (frame.data.size() != 1) {
            LOG(ERROR) << "    查询指令数据长度错误, 期望1, 实际: "
                 << frame.data.size();
            break;
          }

          auto mqtt_manager = mqtt_manager_.lock();
          if (!mqtt_manager) {
            LOG(ERROR) << "    MQTT管理器未初始化，无法回复查询指令";
            break;
          }

          // 构造响应数据域：标识(0xC1) + 电机参数(23字节) + 温度电压参数(15字节)
          std::vector<uint8_t> response_data;
          response_data.reserve(39);
          response_data.push_back(0xC1);

          // 电机参数 (23字节)
          response_data.push_back(static_cast<uint8_t>(data_.motor_params.walk_motor_speed));
          response_data.push_back(static_cast<uint8_t>(data_.motor_params.brush_motor_speed));
          response_data.push_back(static_cast<uint8_t>(data_.motor_params.windproof_motor_speed));

          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.walk_motor_max_current_ma >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.walk_motor_max_current_ma & 0xFF));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.brush_motor_max_current_ma >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.brush_motor_max_current_ma & 0xFF));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.windproof_motor_max_current_ma >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.windproof_motor_max_current_ma & 0xFF));

          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.walk_motor_warning_current_ma >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.walk_motor_warning_current_ma & 0xFF));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.brush_motor_warning_current_ma >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.brush_motor_warning_current_ma & 0xFF));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.windproof_motor_warning_current_ma >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.windproof_motor_warning_current_ma & 0xFF));

          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.walk_motor_mileage_m >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.walk_motor_mileage_m & 0xFF));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.brush_motor_timeout_s >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.brush_motor_timeout_s & 0xFF));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.windproof_motor_timeout_s >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.motor_params.windproof_motor_timeout_s & 0xFF));

          response_data.push_back(static_cast<uint8_t>(data_.motor_params.reverse_time_s));
          response_data.push_back(static_cast<uint8_t>(data_.motor_params.protection_angle));

          // 温度电压参数 (15字节)
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.protection_current_ma >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.protection_current_ma & 0xFF));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.high_temp_threshold));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.low_temp_threshold));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.protection_temp));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.recovery_temp));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.protection_voltage));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.recovery_voltage));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.protection_battery_level));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.limit_run_battery_level));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.recovery_battery_level));

          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.board_protection_temp >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.board_protection_temp & 0xFF));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.board_recovery_temp >> 8));
          response_data.push_back(
            static_cast<uint8_t>(data_.temp_voltage_protection.board_recovery_temp & 0xFF));

          // 回复沿用请求中的编号与帧计数
          std::vector<uint8_t> encoded =
            protocol_.Encode(CONTROL_CODE_DOWNLINK, frame.number,
                     frame.frame_count, response_data);
          std::string base64_data = Protocol::BytesToBase64(encoded);
          std::string payload = GenerateUplinkPayload(base64_data);
          mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

          LOG(INFO) << "    查询回复已发送: "
                << Protocol::BytesToHexString(encoded);
          break;
          }

          case 0xC0: {  // 查询指令：Lora参数&清扫设置
            LOG(INFO) << "    命令类型: 查询Lora参数&清扫设置";

            if (frame.control_code != CONTROL_CODE_UPLINK) {
              LOG(INFO) << "    非平台下发控制码(0x"
                        << std::hex << static_cast<int>(frame.control_code)
                        << ")，忽略查询指令";
              break;
            }

            // 查询指令仅包含标识符
            if (frame.data.size() != 1) {
              LOG(ERROR) << "    查询指令数据长度错误, 期望1, 实际: "
                         << frame.data.size();
              break;
            }

            auto mqtt_manager = mqtt_manager_.lock();
            if (!mqtt_manager) {
              LOG(ERROR) << "    MQTT管理器未初始化，无法回复查询指令";
              break;
            }

            // 构造响应数据域：标识(0xC0) + Lora参数 + 清扫设置
            std::vector<uint8_t> response_data;
            response_data.push_back(0xC0);

            // Lora参数 (3字节)
            response_data.push_back(static_cast<uint8_t>(data_.lora_params.power));
            response_data.push_back(static_cast<uint8_t>(data_.lora_params.frequency));
            response_data.push_back(static_cast<uint8_t>(data_.lora_params.rate));

            // 机器人编号 (2字节)
            uint16_t robot_num = 0;
            try {
              if (!data_.robot_number.empty()) {
                robot_num = static_cast<uint16_t>(std::stoul(data_.robot_number));
              }
            } catch (...) {
              robot_num = 0;
            }
            response_data.push_back(static_cast<uint8_t>(robot_num >> 8));
            response_data.push_back(static_cast<uint8_t>(robot_num & 0xFF));

            // 软件版本 (2字节)
            uint8_t major_version = 1;
            uint8_t minor_version = 0;
            if (!data_.software_version.empty()) {
              sscanf(data_.software_version.c_str(), "%hhu.%hhu", &major_version,
                     &minor_version);
            }
            response_data.push_back(major_version);
            response_data.push_back(minor_version);

            // 启用/停用 + 保留 + 停机位 + 白天防误扫
            response_data.push_back(0x00);
            response_data.push_back(0x00);
            response_data.push_back(static_cast<uint8_t>(data_.parking_position));
            response_data.push_back(data_.daytime_scan_protect ? 0x01 : 0x00);

            // 定时任务1-7 (每组4字节)
            for (int i = 0; i < 7; ++i) {
              const auto& task = data_.schedule_tasks[i];
              response_data.push_back(static_cast<uint8_t>(task.weekday));
              response_data.push_back(static_cast<uint8_t>(task.hour));
              response_data.push_back(static_cast<uint8_t>(task.minute));
              response_data.push_back(static_cast<uint8_t>(task.run_count));
            }

            // 回复沿用请求中的编号与帧计数
            std::vector<uint8_t> encoded =
                protocol_.Encode(CONTROL_CODE_DOWNLINK, frame.number,
                                 frame.frame_count, response_data);
            std::string base64_data = Protocol::BytesToBase64(encoded);
            std::string payload = GenerateUplinkPayload(base64_data);
            mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

            LOG(INFO) << "    查询回复已发送: "
                      << Protocol::BytesToHexString(encoded);
            break;
          }

          case 0xA0: {  // 电机参数设置
            LOG(INFO) << "    命令类型: 电机参数设置";

            if (frame.control_code != CONTROL_CODE_UPLINK) {
              LOG(INFO) << "    非平台下发控制码(0x"
                        << std::hex << static_cast<int>(frame.control_code)
                        << ")，忽略电机参数写入";
              break;
            }

            // 标识(1) + 参数(23) = 24字节
            if (frame.data.size() != 24) {
              LOG(ERROR) << "    电机参数数据长度错误, 期望24, 实际: "
                         << frame.data.size();
              break;
            }

            data_.motor_params.walk_motor_speed = frame.data[1];
            data_.motor_params.brush_motor_speed = frame.data[2];
            data_.motor_params.windproof_motor_speed = frame.data[3];

            data_.motor_params.walk_motor_max_current_ma =
                (static_cast<int>(frame.data[4]) << 8) | frame.data[5];
            data_.motor_params.brush_motor_max_current_ma =
                (static_cast<int>(frame.data[6]) << 8) | frame.data[7];
            data_.motor_params.windproof_motor_max_current_ma =
                (static_cast<int>(frame.data[8]) << 8) | frame.data[9];

            data_.motor_params.walk_motor_warning_current_ma =
                (static_cast<int>(frame.data[10]) << 8) | frame.data[11];
            data_.motor_params.brush_motor_warning_current_ma =
                (static_cast<int>(frame.data[12]) << 8) | frame.data[13];
            data_.motor_params.windproof_motor_warning_current_ma =
                (static_cast<int>(frame.data[14]) << 8) | frame.data[15];

            data_.motor_params.walk_motor_mileage_m =
                (static_cast<int>(frame.data[16]) << 8) | frame.data[17];
            data_.motor_params.brush_motor_timeout_s =
                (static_cast<int>(frame.data[18]) << 8) | frame.data[19];
            data_.motor_params.windproof_motor_timeout_s =
                (static_cast<int>(frame.data[20]) << 8) | frame.data[21];
            data_.motor_params.reverse_time_s = frame.data[22];
            data_.motor_params.protection_angle = frame.data[23];

            LOG(INFO) << "    电机参数已更新 - 行走/毛刷/防风速率: "
                      << data_.motor_params.walk_motor_speed << "/"
                      << data_.motor_params.brush_motor_speed << "/"
                      << data_.motor_params.windproof_motor_speed;

            auto mqtt_manager = mqtt_manager_.lock();
            if (!mqtt_manager) {
              LOG(ERROR) << "    MQTT管理器未初始化，无法回复电机参数设置";
              break;
            }

            // 按协议回复：数据域与下发一致，仅控制码改为0x82
            std::vector<uint8_t> response_data = frame.data;
            std::vector<uint8_t> encoded =
                protocol_.Encode(CONTROL_CODE_DOWNLINK, frame.number,
                                 frame.frame_count, response_data);
            std::string base64_data = Protocol::BytesToBase64(encoded);
            std::string payload = GenerateUplinkPayload(base64_data);
            mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

            LOG(INFO) << "    电机参数设置回复已发送: "
                      << Protocol::BytesToHexString(encoded);

            auto config_db = config_db_.lock();
            if (config_db && !config_db->UpdateRobotDataSnapshot(robot_id_, SerializeDataSnapshot())) {
              LOG(WARNING) << "    电机参数设置后写入快照失败";
            }
            break;
          }

          case 0xA1: {  // 电池参数设置
            LOG(INFO) << "    命令类型: 电池参数设置";

            if (frame.control_code != CONTROL_CODE_UPLINK) {
              LOG(INFO) << "    非平台下发控制码(0x"
                        << std::hex << static_cast<int>(frame.control_code)
                        << ")，忽略电池参数写入";
              break;
            }

            // 标识(1) + 参数(11) = 12字节
            if (frame.data.size() != 12) {
              LOG(ERROR) << "    电池参数数据长度错误, 期望12, 实际: "
                         << frame.data.size();
              break;
            }

            data_.temp_voltage_protection.protection_current_ma =
                (static_cast<int>(frame.data[1]) << 8) | frame.data[2];
            data_.temp_voltage_protection.high_temp_threshold = frame.data[3];
            data_.temp_voltage_protection.low_temp_threshold = frame.data[4];
            data_.temp_voltage_protection.protection_temp = frame.data[5];
            data_.temp_voltage_protection.recovery_temp = frame.data[6];
            data_.temp_voltage_protection.protection_voltage = frame.data[7];
            data_.temp_voltage_protection.recovery_voltage = frame.data[8];
            data_.temp_voltage_protection.protection_battery_level = frame.data[9];
            data_.temp_voltage_protection.limit_run_battery_level = frame.data[10];
            data_.temp_voltage_protection.recovery_battery_level = frame.data[11];

            LOG(INFO) << "    电池参数已更新 - 保护电流(mA): "
                      << data_.temp_voltage_protection.protection_current_ma;

            auto mqtt_manager = mqtt_manager_.lock();
            if (!mqtt_manager) {
              LOG(ERROR) << "    MQTT管理器未初始化，无法回复电池参数设置";
              break;
            }

            std::vector<uint8_t> response_data = frame.data;
            std::vector<uint8_t> encoded =
                protocol_.Encode(CONTROL_CODE_DOWNLINK, frame.number,
                                 frame.frame_count, response_data);
            std::string base64_data = Protocol::BytesToBase64(encoded);
            std::string payload = GenerateUplinkPayload(base64_data);
            mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

            LOG(INFO) << "    电池参数设置回复已发送: "
                      << Protocol::BytesToHexString(encoded);

            auto config_db = config_db_.lock();
            if (config_db &&
                !config_db->UpdateRobotDataSnapshot(robot_id_, SerializeDataSnapshot())) {
              LOG(WARNING) << "    电池参数设置后写入快照失败";
            }
            break;
          }

          case 0xA2: {  // 定时设置
            LOG(INFO) << "    命令类型: 定时设置";

            if (frame.control_code != CONTROL_CODE_UPLINK) {
              LOG(INFO) << "    非平台下发控制码(0x"
                        << std::hex << static_cast<int>(frame.control_code)
                        << ")，忽略定时任务写入";
              break;
            }

            // 标识(1) + 7组定时参数(每组4字节) = 29字节
            if (frame.data.size() != 29) {
              LOG(ERROR) << "    定时设置数据长度错误, 期望29, 实际: "
                         << frame.data.size();
              break;
            }

            if (data_.schedule_tasks.size() < 7) {
              data_.schedule_tasks.resize(7);
            }

            for (size_t i = 0; i < 7; ++i) {
              size_t offset = 1 + i * 4;
              data_.schedule_tasks[i].weekday = frame.data[offset];
              data_.schedule_tasks[i].hour = frame.data[offset + 1];
              data_.schedule_tasks[i].minute = frame.data[offset + 2];
              uint8_t run_count = frame.data[offset + 3];
              data_.schedule_tasks[i].run_count =
                  (run_count < 127) ? static_cast<int>(run_count * 2)
                                    : static_cast<int>(run_count);
            }

            auto mqtt_manager = mqtt_manager_.lock();
            if (!mqtt_manager) {
              LOG(ERROR) << "    MQTT管理器未初始化，无法回复定时设置";
              break;
            }

            // 按协议回复：数据域与下发一致，仅控制码改为0x82；
            // 运行次数字段当值<127时，回包填充为请求值*2。
            std::vector<uint8_t> response_data = frame.data;
            for (size_t i = 0; i < 7; ++i) {
              size_t run_count_index = 1 + i * 4 + 3;
              uint8_t run_count = frame.data[run_count_index];
              if (run_count < 127) {
                response_data[run_count_index] = static_cast<uint8_t>(run_count * 2);
              }
            }

            std::vector<uint8_t> encoded =
                protocol_.Encode(CONTROL_CODE_DOWNLINK, frame.number,
                                 frame.frame_count, response_data);
            std::string base64_data = Protocol::BytesToBase64(encoded);
            std::string payload = GenerateUplinkPayload(base64_data);
            mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

            LOG(INFO) << "    定时设置回复已发送: "
                      << Protocol::BytesToHexString(encoded);

            auto config_db = config_db_.lock();
            if (config_db &&
                !config_db->UpdateRobotDataSnapshot(robot_id_, SerializeDataSnapshot())) {
              LOG(WARNING) << "    定时设置后写入快照失败";
            }
            break;
          }

          case 0xA3: {  // 停机位设置
            LOG(INFO) << "    命令类型: 停机位设置";

            if (frame.control_code != CONTROL_CODE_UPLINK) {
              LOG(INFO) << "    非平台下发控制码(0x"
                        << std::hex << static_cast<int>(frame.control_code)
                        << ")，忽略停机位写入";
              break;
            }

            // 标识(1) + 参数(1) = 2字节
            if (frame.data.size() != 2) {
              LOG(ERROR) << "    停机位设置数据长度错误, 期望2, 实际: "
                         << frame.data.size();
              break;
            }

            data_.parking_position = frame.data[1];
            LOG(INFO) << "    停机位已更新: " << data_.parking_position;

            auto mqtt_manager = mqtt_manager_.lock();
            if (!mqtt_manager) {
              LOG(ERROR) << "    MQTT管理器未初始化，无法回复停机位设置";
              break;
            }

            // 按协议回复：数据域与下发一致，仅控制码改为0x82
            std::vector<uint8_t> response_data = frame.data;
            std::vector<uint8_t> encoded =
                protocol_.Encode(CONTROL_CODE_DOWNLINK, frame.number,
                                 frame.frame_count, response_data);
            std::string base64_data = Protocol::BytesToBase64(encoded);
            std::string payload = GenerateUplinkPayload(base64_data);
            mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

            LOG(INFO) << "    停机位设置回复已发送: "
                      << Protocol::BytesToHexString(encoded);

            auto config_db = config_db_.lock();
            if (config_db &&
                !config_db->UpdateRobotDataSnapshot(robot_id_, SerializeDataSnapshot())) {
              LOG(WARNING) << "    停机位设置后写入快照失败";
            }
            break;
          }

          case 0xA8: {  // 广播参数设置
            LOG(INFO) << "    命令类型: 广播参数设置";

            if (frame.control_code != CONTROL_CODE_UPLINK) {
              LOG(INFO) << "    非平台下发控制码(0x"
                        << std::hex << static_cast<int>(frame.control_code)
                        << ")，忽略广播参数写入";
              break;
            }

            // 标识(1) + 时间(7) + 风速(1) + 通信箱数量(2) + 机器人数量(2) + 后台保护(1) = 14字节
            if (frame.data.size() != 14) {
              LOG(ERROR) << "    广播参数设置数据长度错误, 期望14, 实际: "
                         << frame.data.size();
              break;
            }

            uint8_t year = frame.data[1];
            uint8_t month = frame.data[2];
            uint8_t day = frame.data[3];
            uint8_t hour = frame.data[4];
            uint8_t minute = frame.data[5];
            uint8_t second = frame.data[6];
            uint8_t weekday = frame.data[7];
            uint8_t wind_speed = frame.data[8];

            uint16_t comm_box_count =
                (static_cast<uint16_t>(frame.data[9]) << 8) | frame.data[10];
            uint16_t robot_count =
                (static_cast<uint16_t>(frame.data[11]) << 8) | frame.data[12];
            uint8_t protection_info = frame.data[13];

            data_.local_time.year = 2000 + year;
            data_.local_time.month = month;
            data_.local_time.day = day;
            data_.local_time.hour = hour;
            data_.local_time.minute = minute;
            data_.local_time.second = second;
            data_.local_time.weekday = weekday;
            data_.current_timestamp.hour = hour;
            data_.current_timestamp.minute = minute;
            data_.current_timestamp.second = second;

            LOG(INFO) << "    广播参数已更新 - 时间: 20" << std::setfill('0')
                      << std::setw(2) << static_cast<int>(year) << "-"
                      << std::setw(2) << static_cast<int>(month) << "-"
                      << std::setw(2) << static_cast<int>(day) << " "
                      << std::setw(2) << static_cast<int>(hour) << ":"
                      << std::setw(2) << static_cast<int>(minute) << ":"
                      << std::setw(2) << static_cast<int>(second)
                      << " 星期" << static_cast<int>(weekday)
                      << " 风速=" << static_cast<int>(wind_speed)
                      << " 通信箱=" << comm_box_count
                      << " 机器人数=" << robot_count
                      << " 保护位=0x" << std::hex
                      << static_cast<int>(protection_info);

            // 广播指令按协议不回复
            LOG(INFO) << "    广播参数设置按协议不回复";

            auto config_db = config_db_.lock();
            if (config_db &&
                !config_db->UpdateRobotDataSnapshot(robot_id_, SerializeDataSnapshot())) {
              LOG(WARNING) << "    广播参数设置后写入快照失败";
            }
            break;
          }

          case 0xA4:  // LoRa参数设置
            LOG(INFO) << "    命令类型: LoRa参数设置";
            // TODO: 解析参数并更新配置
            break;

          case 0xF0: {  // 定时启动请求回复
            LOG(INFO) << "    命令类型: 定时启动请求回复";
            if (frame.data.size() >= 15) {  // 标识(1) + 参数(14)
              uint8_t start_flag = frame.data[1];           // 启动运行标志
              uint8_t year = frame.data[2];                 // 年
              uint8_t month = frame.data[3];                // 月
              uint8_t day = frame.data[4];                  // 日
              uint8_t hour = frame.data[5];                 // 时
              uint8_t minute = frame.data[6];               // 分
              uint8_t second = frame.data[7];               // 秒
              uint8_t weekday = frame.data[8];              // 星期
              uint8_t wind_speed = frame.data[9];           // 当前风速
              uint16_t comm_box_count = (static_cast<uint16_t>(frame.data[10]) << 8) | frame.data[11];  // 通信箱数量
              uint16_t robot_count = (static_cast<uint16_t>(frame.data[12]) << 8) | frame.data[13];     // 机器人数量
              uint8_t protection_info = frame.data[14];     // 后台保护信息

              data_.request_reply.available = true;
              data_.request_reply.start_flag = start_flag;
              data_.request_reply.year = year;
              data_.request_reply.month = month;
              data_.request_reply.day = day;
              data_.request_reply.hour = hour;
              data_.request_reply.minute = minute;
              data_.request_reply.second = second;
              data_.request_reply.weekday = weekday;
              data_.request_reply.wind_speed = wind_speed;
              data_.request_reply.comm_box_count = comm_box_count;
              data_.request_reply.robot_count = robot_count;
              data_.request_reply.protection_info = protection_info;

              LOG(INFO) << "    === 定时启动请求回复解析 ===";
              LOG(INFO) << "    启动运行标志: 0x" << std::hex << static_cast<int>(start_flag);
              LOG(INFO) << "    时间信息: 20" << std::dec << static_cast<int>(year)
                       << "-" << std::setfill('0') << std::setw(2) << static_cast<int>(month)
                       << "-" << std::setw(2) << static_cast<int>(day)
                       << " " << std::setw(2) << static_cast<int>(hour)
                       << ":" << std::setw(2) << static_cast<int>(minute)
                       << ":" << std::setw(2) << static_cast<int>(second)
                       << " 星期" << static_cast<int>(weekday);
              LOG(INFO) << "    当前风速: " << static_cast<int>(wind_speed);
              LOG(INFO) << "    通信箱数量: " << comm_box_count;
              LOG(INFO) << "    机器人数量: " << robot_count;
              LOG(INFO) << "    后台保护信息: 0x" << std::hex << static_cast<int>(protection_info);
              LOG(INFO) << "      - 大风保护: " << (IsWindProtectionEnabled(protection_info) ? "开启" : "关闭");
              LOG(INFO) << "      - 湿度保护: " << (IsHumidityProtectionEnabled(protection_info) ? "开启" : "关闭");
              LOG(INFO) << "      - 支架保护: " << (IsBracketProtectionEnabled(protection_info) ? "开启" : "关闭");
              LOG(INFO) << "      - 环境温度保护: " << (IsAmbientTemperatureProtectionEnabled(protection_info) ? "开启" : "关闭");
              MarkRequestReplyReceived();
            } else {
              LOG(ERROR) << "    定时启动回复数据长度不足";
            }
            break;
          }

          case 0xF1: {  // 启动请求回复
            LOG(INFO) << "    命令类型: 启动请求回复";
            if (frame.data.size() >= 15) {  // 标识(1) + 参数(14)
              uint8_t start_flag = frame.data[1];           // 启动运行标志
              uint8_t year = frame.data[2];                 // 年
              uint8_t month = frame.data[3];                // 月
              uint8_t day = frame.data[4];                  // 日
              uint8_t hour = frame.data[5];                 // 时
              uint8_t minute = frame.data[6];               // 分
              uint8_t second = frame.data[7];               // 秒
              uint8_t weekday = frame.data[8];              // 星期
              uint8_t wind_speed = frame.data[9];           // 当前风速
              uint16_t comm_box_count = (static_cast<uint16_t>(frame.data[10]) << 8) | frame.data[11];  // 通信箱数量
              uint16_t robot_count = (static_cast<uint16_t>(frame.data[12]) << 8) | frame.data[13];     // 机器人数量
              uint8_t protection_info = frame.data[14];     // 后台保护信息

              data_.request_reply.available = true;
              data_.request_reply.start_flag = start_flag;
              data_.request_reply.year = year;
              data_.request_reply.month = month;
              data_.request_reply.day = day;
              data_.request_reply.hour = hour;
              data_.request_reply.minute = minute;
              data_.request_reply.second = second;
              data_.request_reply.weekday = weekday;
              data_.request_reply.wind_speed = wind_speed;
              data_.request_reply.comm_box_count = comm_box_count;
              data_.request_reply.robot_count = robot_count;
              data_.request_reply.protection_info = protection_info;

              LOG(INFO) << "    === 启动请求回复解析 ===";
              LOG(INFO) << "    启动运行标志: 0x" << std::hex << static_cast<int>(start_flag);
              LOG(INFO) << "    时间信息: 20" << std::dec << static_cast<int>(year)
                       << "-" << std::setfill('0') << std::setw(2) << static_cast<int>(month)
                       << "-" << std::setw(2) << static_cast<int>(day)
                       << " " << std::setw(2) << static_cast<int>(hour)
                       << ":" << std::setw(2) << static_cast<int>(minute)
                       << ":" << std::setw(2) << static_cast<int>(second)
                       << " 星期" << static_cast<int>(weekday);
              LOG(INFO) << "    当前风速: " << static_cast<int>(wind_speed);
              LOG(INFO) << "    通信箱数量: " << comm_box_count;
              LOG(INFO) << "    机器人数量: " << robot_count;
              LOG(INFO) << "    后台保护信息: 0x" << std::hex << static_cast<int>(protection_info);
              LOG(INFO) << "      - 大风保护: " << (IsWindProtectionEnabled(protection_info) ? "开启" : "关闭");
              LOG(INFO) << "      - 湿度保护: " << (IsHumidityProtectionEnabled(protection_info) ? "开启" : "关闭");
              LOG(INFO) << "      - 支架保护: " << (IsBracketProtectionEnabled(protection_info) ? "开启" : "关闭");
              LOG(INFO) << "      - 环境温度保护: " << (IsAmbientTemperatureProtectionEnabled(protection_info) ? "开启" : "关闭");
              MarkRequestReplyReceived();
            } else {
              LOG(ERROR) << "    启动请求回复数据长度不足";
            }
            break;
          }

          case 0xF2: {  // 校时请求回复
            LOG(INFO) << "    命令类型: 校时请求回复";
            if (frame.data.size() >= 14) {  // 标识(1) + 参数(13)
              uint8_t year = frame.data[1];                 // 年
              uint8_t month = frame.data[2];                // 月
              uint8_t day = frame.data[3];                  // 日
              uint8_t hour = frame.data[4];                 // 时
              uint8_t minute = frame.data[5];               // 分
              uint8_t second = frame.data[6];               // 秒
              uint8_t weekday = frame.data[7];              // 星期
              uint8_t wind_speed = frame.data[8];           // 当前风速
              uint16_t comm_box_count = (static_cast<uint16_t>(frame.data[9]) << 8) | frame.data[10];   // 通信箱数量
              uint16_t robot_count = (static_cast<uint16_t>(frame.data[11]) << 8) | frame.data[12];    // 机器人数量
              uint8_t protection_info = frame.data[13];     // 后台保护信息

              data_.request_reply.available = true;
              data_.request_reply.start_flag = 0;
              data_.request_reply.year = year;
              data_.request_reply.month = month;
              data_.request_reply.day = day;
              data_.request_reply.hour = hour;
              data_.request_reply.minute = minute;
              data_.request_reply.second = second;
              data_.request_reply.weekday = weekday;
              data_.request_reply.wind_speed = wind_speed;
              data_.request_reply.comm_box_count = comm_box_count;
              data_.request_reply.robot_count = robot_count;
              data_.request_reply.protection_info = protection_info;

              LOG(INFO) << "    === 校时请求回复解析 ===";
              LOG(INFO) << "    时间信息: 20" << std::dec << static_cast<int>(year)
                       << "-" << std::setfill('0') << std::setw(2) << static_cast<int>(month)
                       << "-" << std::setw(2) << static_cast<int>(day)
                       << " " << std::setw(2) << static_cast<int>(hour)
                       << ":" << std::setw(2) << static_cast<int>(minute)
                       << ":" << std::setw(2) << static_cast<int>(second)
                       << " 星期" << static_cast<int>(weekday);
              LOG(INFO) << "    当前风速: " << static_cast<int>(wind_speed);
              LOG(INFO) << "    通信箱数量: " << comm_box_count;
              LOG(INFO) << "    机器人数量: " << robot_count;
              LOG(INFO) << "    后台保护信息: 0x" << std::hex << static_cast<int>(protection_info);
              LOG(INFO) << "      - 大风保护: " << (IsWindProtectionEnabled(protection_info) ? "开启" : "关闭");
              LOG(INFO) << "      - 湿度保护: " << (IsHumidityProtectionEnabled(protection_info) ? "开启" : "关闭");
              LOG(INFO) << "      - 支架保护: " << (IsBracketProtectionEnabled(protection_info) ? "开启" : "关闭");
              LOG(INFO) << "      - 环境温度保护: " << (IsAmbientTemperatureProtectionEnabled(protection_info) ? "开启" : "关闭");
              MarkRequestReplyReceived();
            } else {
              LOG(ERROR) << "    校时请求回复数据长度不足";
            }
            break;
          }

          // 控制类指令 (0xB0-0xB6)
          case 0xB0:  // 启用/解锁
            LOG(INFO) << "    命令类型: 启用/解锁";
            ControlEnable();
            SendControlResponse(0xB0);
            break;

          case 0xB1:  // 停用/锁定
            LOG(INFO) << "    命令类型: 停用/锁定";
            ControlDisable();
            SendControlResponse(0xB1);
            break;

          case 0xB2:  // 启动
            LOG(INFO) << "    命令类型: 启动";
            ControlStart();
            SendControlResponse(0xB2);
            break;

          case 0xB3:  // 前进
            LOG(INFO) << "    命令类型: 前进";
            ControlForward();
            SendControlResponse(0xB3);
            break;

          case 0xB4:  // 后退
            LOG(INFO) << "    命令类型: 后退";
            ControlBackward();
            SendControlResponse(0xB4);
            break;

          case 0xB5:  // 停止
            LOG(INFO) << "    命令类型: 停止";
            ControlStop();
            SendControlResponse(0xB5);
            break;

          case 0xB6:  // 复位
            LOG(INFO) << "    命令类型: 复位";
            SendControlResponse(0xB6);
            break;

          case 0xBA:  // 重启指令
            LOG(INFO) << "    命令类型: 重启";
            // 发送简单响应（只包含标识符，不含机器人数据）
            SendRestartResponse(0xBA);
            break;

          default:
            LOG(WARNING) << "    未知命令标识: 0x" << std::hex << static_cast<int>(identifier);
            break;
        }
      }
    } else {
      LOG(ERROR) << "  协议解析失败";
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "  处理消息异常: " << e.what();
  }
}

void Robot::SetReportInterval(int interval_seconds) {
  robot_data_report_interval_s_ = interval_seconds;
  LOG(INFO) << "[Robot " << robot_id_ << "] 设置机器人数据上报间隔为 " << interval_seconds << " 秒";
}

void Robot::SetReportIntervals(int robot_data_s, int motor_params_s, int lora_clean_s) {
  robot_data_report_interval_s_ = robot_data_s;
  motor_params_report_interval_s_ = motor_params_s;
  lora_clean_report_interval_s_ = lora_clean_s;
  LOG(INFO) << "[Robot " << robot_id_ << "] 设置上报间隔 - 机器人数据:" << robot_data_s
            << "s, 电机参数:" << motor_params_s << "s, Lora&清扫设置:" << lora_clean_s << "s";
}

void Robot::SetRobotIndex(int index) {
  robot_index_ = index;
  LOG(INFO) << "[Robot " << robot_id_ << "] 设置机器人索引: " << index;
}

void Robot::StartReport() {
  // 如果线程已经在运行，先停止
  if (report_thread_.joinable()) {
    LOG(WARNING) << "[Robot " << robot_id_ << "] 上报线程已在运行，先停止";
    StopReport();
  }

  // 重置停止标志
  stop_report_.store(false);

  // 启动新线程
  report_thread_ = std::thread(&Robot::ReportThreadFunc, this);
  LOG(INFO) << "[Robot " << robot_id_ << "] 定时上报已启动";
}

void Robot::StopReport() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 正在停止定时上报...";
  stop_report_.store(true);

  if (report_thread_.joinable()) {
    report_thread_.join();
    LOG(INFO) << "[Robot " << robot_id_ << "] 定时上报已停止";
  }
}

void Robot::UpdateSimulatedData() {
  if (!data_.sim_config.enabled) return;
  const auto& sim = data_.sim_config;
  const int dir = move_direction_.load();
  const bool is_moving = (dir != 0);
  const bool at_dock   = (data_.position == 0 && !is_moving);

  auto rand_val = [&](float lo, float hi) -> float {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
  };

  // 主/从电机电流（运行时有值，否则为0）
  if (is_moving) {
    float main_a  = sim.main_current_random
                    ? rand_val(sim.main_current_min, sim.main_current_max)
                    : sim.main_current_fixed;
    float slave_a = sim.slave_current_random
                    ? rand_val(sim.slave_current_min, sim.slave_current_max)
                    : sim.slave_current_fixed;
    data_.main_motor_current  = static_cast<int>(std::round(main_a  * 10.0f));
    data_.slave_motor_current = static_cast<int>(std::round(slave_a * 10.0f));
  } else {
    data_.main_motor_current  = 0;
    data_.slave_motor_current = 0;
  }

  // 光伏输出（停靠时有值，否则为0）
  if (at_dock) {
    float sv = sim.solar_voltage_random
               ? rand_val(sim.solar_voltage_min, sim.solar_voltage_max)
               : sim.solar_voltage_fixed;
    float sc = sim.solar_current_random
               ? rand_val(sim.solar_current_min, sim.solar_current_max)
               : sim.solar_current_fixed;
    data_.solar_voltage = static_cast<int>(std::round(sv * 10.0f));
    data_.solar_current = static_cast<int>(std::round(sc * 10.0f));
  } else {
    data_.solar_voltage = 0;
    data_.solar_current = 0;
  }

  // 主板温度（始终有值）
  {
    float bt = sim.board_temp_random
               ? rand_val(sim.board_temp_min, sim.board_temp_max)
               : sim.board_temp_fixed;
    data_.board_temperature = static_cast<int>(std::round(bt));
  }

  // 电池电压（始终有值）
  {
    float bv = sim.battery_voltage_random
               ? rand_val(sim.battery_voltage_min, sim.battery_voltage_max)
               : sim.battery_voltage_fixed;
    data_.battery_voltage = static_cast<int>(std::round(bv * 10.0f));
  }

  // 电池电流 = 主 + 从电机电流
  data_.battery_current = data_.main_motor_current + data_.slave_motor_current;

  // 电池温度（始终有值）
  {
    float t = sim.battery_temp_random
              ? rand_val(sim.battery_temp_min, sim.battery_temp_max)
              : sim.battery_temp_fixed;
    data_.battery_temperature = static_cast<int>(std::round(t));
  }

  // 电池电量（每600 tick 即每分钟更新一次）
  ++battery_level_tick_;
  if (battery_level_tick_ >= 600) {
    battery_level_tick_ = 0;
    if (is_moving) {
      battery_level_f_ -= sim.battery_discharge_run;
    } else if (at_dock) {
      battery_level_f_ += sim.battery_charge_rate;
    } else {
      battery_level_f_ -= sim.battery_discharge_stop;
    }
    battery_level_f_ = std::clamp(battery_level_f_, 0.0f, 100.0f);
    data_.battery_level = static_cast<int>(battery_level_f_);
  }

  // 告警模拟
  ++total_ticks_;
  ++alarm_sim_tick_;
  const auto& alm = data_.sim_config.alarm_sim;

  // 清除到期的模拟告警
  for (auto it = alarm_entries_.begin(); it != alarm_entries_.end(); ) {
    if (total_ticks_ >= it->expire_tick) {
      data_.alarm_fc &= ~(static_cast<uint32_t>(1) << it->bit);
      it = alarm_entries_.erase(it);
    } else {
      ++it;
    }
  }

  // 每6000 tick（10分钟）产生新告警
  if (alm.enabled && alarm_sim_tick_ >= 6000) {
    alarm_sim_tick_ = 0;
    std::vector<int> available_bits;
    for (int b = 0; b < 32; ++b) {
      if (alm.fc_bits_mask & (static_cast<uint32_t>(1) << b)) {
        available_bits.push_back(b);
      }
    }
    if (!available_bits.empty()) {
      std::uniform_int_distribution<int> bit_dist(0, static_cast<int>(available_bits.size()) - 1);
      int dur_lo = std::min(alm.duration_min, alm.duration_max);
      int dur_hi = std::max(alm.duration_min, alm.duration_max);
      if (dur_lo == dur_hi) ++dur_hi;
      std::uniform_int_distribution<int> dur_dist(dur_lo, dur_hi);
      for (int i = 0; i < alm.frequency; ++i) {
        int bit    = available_bits[bit_dist(rng_)];
        int expire = total_ticks_ + dur_dist(rng_) * 600;
        data_.alarm_fc |= (static_cast<uint32_t>(1) << bit);
        alarm_entries_.push_back({bit, expire});
      }
    }
  }
}

void Robot::ReportThreadFunc() {
  // 初始化浮点电量
  battery_level_f_ = static_cast<float>(data_.battery_level);
  // 实时计算错峰偏移：索引 × 间隔 / 总数（在线程启动时从 MqttManager 获取总数）
  int total_robots = 1;
  if (auto mgr = mqtt_manager_.lock()) {
    total_robots = mgr->GetRobotCount();
    if (total_robots <= 0) total_robots = 1;
  }
  const int rd_offset_ticks = robot_index_ * robot_data_report_interval_s_    * 10 / total_robots;
  const int mp_offset_ticks = robot_index_ * motor_params_report_interval_s_  * 10 / total_robots;
  const int lc_offset_ticks = robot_index_ * lora_clean_report_interval_s_    * 10 / total_robots;

  LOG(INFO) << "[Robot " << robot_id_ << "] 上报线程已启动 - 机器人数据:" << robot_data_report_interval_s_
            << "s(+" << rd_offset_ticks / 10 << "s错峰), 电机参数:" << motor_params_report_interval_s_
            << "s(+" << mp_offset_ticks / 10 << "s错峰), Lora&清扫:"
            << lora_clean_report_interval_s_ << "s(+" << lc_offset_ticks / 10 << "s错峰)"
            << " [索引" << robot_index_ << "/" << total_robots << "]";

  // 各类型上报的计时器（初始为负数以实现错峰，累加到间隔ticks时触发）
  int robot_data_ticks    = -rd_offset_ticks;
  int motor_params_ticks  = -mp_offset_ticks;
  int lora_clean_ticks    = -lc_offset_ticks;

  while (!stop_report_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (stop_report_.load()) break;

    ++robot_data_ticks;
    ++motor_params_ticks;
    ++lora_clean_ticks;
    ++position_tick_;

    // 模拟数据更新（每 tick）
    UpdateSimulatedData();

    // 位置更新：前进/后退时每秒扫描 1 个单位
    if (position_tick_ >= 10) {
      position_tick_ = 0;
      int dir = move_direction_.load();
      if (dir != 0) {
        data_.position += dir;
        if (data_.position < 0) data_.position = 0;
      }
    }
    // 每 tick 直接读成员变量，途中修改间隔立即生效
    // 机器人数据上报
    if (robot_data_ticks >= robot_data_report_interval_s_ * 10) {
      robot_data_ticks = 0;
      SendRobotDataReport();
    }

    // 电机参数上报
    if (motor_params_ticks >= motor_params_report_interval_s_ * 10) {
      motor_params_ticks = 0;
      SendMotorParamsReport();
    }

    // Lora参数&清扫设置上报
    if (lora_clean_ticks >= lora_clean_report_interval_s_ * 10) {
      lora_clean_ticks = 0;
      SendLoraAndCleanSettingsReport();
    }
  }

  LOG(INFO) << "[Robot " << robot_id_ << "] 上报线程已停止";
}

// 更新时间相关字段（本地时间、当前时间戳、工作时长）
void Robot::UpdateTimeFields() {
  using namespace std::chrono;
  auto now = system_clock::now();
  std::time_t t = system_clock::to_time_t(now);
  std::tm tm;
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif

  data_.local_time.year = tm.tm_year + 1900;
  data_.local_time.month = tm.tm_mon + 1;
  data_.local_time.day = tm.tm_mday;
  data_.local_time.hour = tm.tm_hour;
  data_.local_time.minute = tm.tm_min;
  data_.local_time.second = tm.tm_sec;
  data_.local_time.weekday = tm.tm_wday;  // 0-6

  data_.current_timestamp.hour = tm.tm_hour;
  data_.current_timestamp.minute = tm.tm_min;
  data_.current_timestamp.second = tm.tm_sec;

  // 工作时长：以小时为单位，从创建时间算起
  if (creation_time_.time_since_epoch().count() > 0) {
    auto dur = duration_cast<hours>(now - creation_time_);
    data_.working_duration = static_cast<int>(dur.count());
  } else {
    data_.working_duration = 0;
  }
}

// 构建机器人数据域（标识符 + 46字节机器人状态数据）
std::vector<uint8_t> Robot::BuildRobotDataField(uint8_t identifier) {
  // 在构建前更新时间字段与工作时长
  UpdateTimeFields();

  std::vector<uint8_t> data_field;

  // 标识符
  data_field.push_back(identifier);

  // FA告警 (4字节)
  uint32_t fa = data_.alarm_fa;
  data_field.push_back(static_cast<uint8_t>(fa >> 24));
  data_field.push_back(static_cast<uint8_t>(fa >> 16));
  data_field.push_back(static_cast<uint8_t>(fa >> 8));
  data_field.push_back(static_cast<uint8_t>(fa));

  // FB告警 (2字节)
  uint16_t fb = data_.alarm_fb;
  data_field.push_back(static_cast<uint8_t>(fb >> 8));
  data_field.push_back(static_cast<uint8_t>(fb));

  // FC告警 (4字节)
  uint32_t fc = data_.alarm_fc;
  data_field.push_back(static_cast<uint8_t>(fc >> 24));
  data_field.push_back(static_cast<uint8_t>(fc >> 16));
  data_field.push_back(static_cast<uint8_t>(fc >> 8));
  data_field.push_back(static_cast<uint8_t>(fc));

  // FD告警 (2字节)
  uint16_t fd = data_.alarm_fd;
  data_field.push_back(static_cast<uint8_t>(fd >> 8));
  data_field.push_back(static_cast<uint8_t>(fd));

  // 主电机电流 (2字节，单位100mA)
  uint16_t main_current = static_cast<uint16_t>(data_.main_motor_current);
  data_field.push_back(static_cast<uint8_t>(main_current >> 8));
  data_field.push_back(static_cast<uint8_t>(main_current));

  // 从电机电流 (2字节，单位100mA)
  uint16_t slave_current = static_cast<uint16_t>(data_.slave_motor_current);
  data_field.push_back(static_cast<uint8_t>(slave_current >> 8));
  data_field.push_back(static_cast<uint8_t>(slave_current));

  // 电池电压 (2字节，单位100mV)
  uint16_t battery_volt = static_cast<uint16_t>(data_.battery_voltage);
  data_field.push_back(static_cast<uint8_t>(battery_volt >> 8));
  data_field.push_back(static_cast<uint8_t>(battery_volt));

  // 电池电流 (2字节，单位100mA)
  uint16_t battery_curr = static_cast<uint16_t>(data_.battery_current);
  data_field.push_back(static_cast<uint8_t>(battery_curr >> 8));
  data_field.push_back(static_cast<uint8_t>(battery_curr));

  // 电池状态 (2字节)
  uint16_t battery_status = static_cast<uint16_t>(data_.battery_status);
  data_field.push_back(static_cast<uint8_t>(battery_status >> 8));
  data_field.push_back(static_cast<uint8_t>(battery_status));

  // 电池电量 (2字节)
  uint16_t battery_level = static_cast<uint16_t>(data_.battery_level);
  data_field.push_back(static_cast<uint8_t>(battery_level >> 8));
  data_field.push_back(static_cast<uint8_t>(battery_level));

  // 电池温度 (2字节)
  uint16_t battery_temp = static_cast<uint16_t>(data_.battery_temperature);
  data_field.push_back(static_cast<uint8_t>(battery_temp >> 8));
  data_field.push_back(static_cast<uint8_t>(battery_temp));

  // 位置信息 (2字节) - 使用position字段
  uint16_t position = static_cast<uint16_t>(data_.position);
  data_field.push_back(static_cast<uint8_t>(position >> 8));
  data_field.push_back(static_cast<uint8_t>(position));

  // 工作时长 (2字节)
  uint16_t work_duration = static_cast<uint16_t>(data_.working_duration);
  data_field.push_back(static_cast<uint8_t>(work_duration >> 8));
  data_field.push_back(static_cast<uint8_t>(work_duration));

  // 光伏板输出电压 (2字节，单位100mV)
  uint16_t solar_volt = static_cast<uint16_t>(data_.solar_voltage);
  data_field.push_back(static_cast<uint8_t>(solar_volt >> 8));
  data_field.push_back(static_cast<uint8_t>(solar_volt));

  // 光伏板输出电流 (2字节，单位100mA)
  uint16_t solar_curr = static_cast<uint16_t>(data_.solar_current);
  data_field.push_back(static_cast<uint8_t>(solar_curr >> 8));
  data_field.push_back(static_cast<uint8_t>(solar_curr));

  // 累计运行次数 (2字节)
  uint16_t total_count = static_cast<uint16_t>(data_.total_run_count);
  data_field.push_back(static_cast<uint8_t>(total_count >> 8));
  data_field.push_back(static_cast<uint8_t>(total_count));

  // 当前运行圈数 (4字节)
  uint32_t lap_count = static_cast<uint32_t>(data_.current_lap_count);
  data_field.push_back(static_cast<uint8_t>(lap_count >> 24));
  data_field.push_back(static_cast<uint8_t>(lap_count >> 16));
  data_field.push_back(static_cast<uint8_t>(lap_count >> 8));
  data_field.push_back(static_cast<uint8_t>(lap_count));

  // 当前实际时间戳 (3字节: 时、分、秒)
  data_field.push_back(static_cast<uint8_t>(data_.current_timestamp.hour));
  data_field.push_back(static_cast<uint8_t>(data_.current_timestamp.minute));
  data_field.push_back(static_cast<uint8_t>(data_.current_timestamp.second));

  // 主板温度 (2字节)
  uint16_t board_temp = static_cast<uint16_t>(data_.board_temperature);
  data_field.push_back(static_cast<uint8_t>(board_temp >> 8));
  data_field.push_back(static_cast<uint8_t>(board_temp));

  return data_field;
}

void Robot::SendMotorParamsRequest(
    uint8_t walk_motor_speed, uint8_t brush_motor_speed,
    uint8_t windproof_motor_speed, uint16_t walk_motor_max_current_ma,
    uint16_t brush_motor_max_current_ma, uint16_t windproof_motor_max_current_ma,
    uint16_t walk_motor_warning_current_ma,
    uint16_t brush_motor_warning_current_ma,
    uint16_t windproof_motor_warning_current_ma, uint16_t walk_motor_mileage_m,
    uint16_t brush_motor_timeout_s, uint16_t windproof_motor_timeout_s,
    uint8_t reverse_time_s, uint8_t protection_angle) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送电机参数设置请求";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  std::vector<uint8_t> data_field = {
      0xA0,  // 标识：电机参数设置
      walk_motor_speed,
      brush_motor_speed,
      windproof_motor_speed,
      static_cast<uint8_t>(walk_motor_max_current_ma >> 8),
      static_cast<uint8_t>(walk_motor_max_current_ma & 0xFF),
      static_cast<uint8_t>(brush_motor_max_current_ma >> 8),
      static_cast<uint8_t>(brush_motor_max_current_ma & 0xFF),
      static_cast<uint8_t>(windproof_motor_max_current_ma >> 8),
      static_cast<uint8_t>(windproof_motor_max_current_ma & 0xFF),
      static_cast<uint8_t>(walk_motor_warning_current_ma >> 8),
      static_cast<uint8_t>(walk_motor_warning_current_ma & 0xFF),
      static_cast<uint8_t>(brush_motor_warning_current_ma >> 8),
      static_cast<uint8_t>(brush_motor_warning_current_ma & 0xFF),
      static_cast<uint8_t>(windproof_motor_warning_current_ma >> 8),
      static_cast<uint8_t>(windproof_motor_warning_current_ma & 0xFF),
      static_cast<uint8_t>(walk_motor_mileage_m >> 8),
      static_cast<uint8_t>(walk_motor_mileage_m & 0xFF),
      static_cast<uint8_t>(brush_motor_timeout_s >> 8),
      static_cast<uint8_t>(brush_motor_timeout_s & 0xFF),
      static_cast<uint8_t>(windproof_motor_timeout_s >> 8),
      static_cast<uint8_t>(windproof_motor_timeout_s & 0xFF),
      reverse_time_s,
      protection_angle,
  };

  uint16_t robot_num = 0;
  try {
    if (!data_.robot_number.empty()) {
      robot_num = static_cast<uint16_t>(std::stoul(data_.robot_number));
    }
  } catch (...) {
    robot_num = 0;
  }

  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded =
      protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_num, frame_count, data_field);

  LOG(INFO) << "  电机参数设置编码后数据: " << Protocol::BytesToHexString(encoded);

  std::string base64_data = Protocol::BytesToBase64(encoded);
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  sequence_.fetch_add(1);
}

void Robot::SendBatteryParamsRequest(uint16_t protection_current_ma,
                                     uint8_t high_temp_threshold,
                                     uint8_t low_temp_threshold,
                                     uint8_t protection_temp,
                                     uint8_t recovery_temp,
                                     uint8_t protection_voltage,
                                     uint8_t recovery_voltage,
                                     uint8_t protection_battery_level,
                                     uint8_t limit_run_battery_level,
                                     uint8_t recovery_battery_level) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送电池参数设置请求";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  std::vector<uint8_t> data_field = {
      0xA1,
      static_cast<uint8_t>(protection_current_ma >> 8),
      static_cast<uint8_t>(protection_current_ma & 0xFF),
      high_temp_threshold,
      low_temp_threshold,
      protection_temp,
      recovery_temp,
      protection_voltage,
      recovery_voltage,
      protection_battery_level,
      limit_run_battery_level,
      recovery_battery_level,
  };

  uint16_t robot_num = 0;
  try {
    if (!data_.robot_number.empty()) {
      robot_num = static_cast<uint16_t>(std::stoul(data_.robot_number));
    }
  } catch (...) {
    robot_num = 0;
  }

  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded =
      protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_num, frame_count, data_field);

  LOG(INFO) << "  电池参数设置编码后数据: " << Protocol::BytesToHexString(encoded);

  std::string base64_data = Protocol::BytesToBase64(encoded);
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  sequence_.fetch_add(1);
}

void Robot::SendScheduleStartRequest(uint8_t schedule_id, uint8_t weekday,
                                     uint8_t hour, uint8_t minute, uint8_t run_count) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送定时启动请求";
  LOG(INFO) << "  定时信息编号: " << static_cast<int>(schedule_id);
  LOG(INFO) << "  星期: " << static_cast<int>(weekday);
  LOG(INFO) << "  时间: " << std::setfill('0') << std::setw(2) << static_cast<int>(hour)
           << ":" << std::setw(2) << static_cast<int>(minute);
  LOG(INFO) << "  运行次数: " << static_cast<int>(run_count);

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(0xF0) + 参数(5字节)
  std::vector<uint8_t> data_field = {
    0xF0,         // 标识：定时启动请求
    schedule_id,  // 定时信息编号
    weekday,      // 星期
    hour,         // 时
    minute,       // 分
    run_count     // 运行次数
  };

  // 使用Protocol编码：控制码0x82（机器人主动发送请求）
  uint16_t robot_num = 0;
  try {
    if (!data_.robot_number.empty()) robot_num = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_num = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_num, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  // 转换为Base64
  std::string base64_data = Protocol::BytesToBase64(encoded);
  LOG(INFO) << "  Base64编码: " << base64_data;

  // 填入上行模板并发送
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  定时启动请求已加入发送队列";

  // 帧计数累加
  sequence_.fetch_add(1);
}

void Robot::SendScheduleParamsRequest(const std::vector<ScheduleTask>& tasks) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送定时设置请求";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  std::vector<ScheduleTask> normalized_tasks(7);
  for (size_t i = 0; i < 7; ++i) {
    if (i < tasks.size()) {
      normalized_tasks[i] = tasks[i];
    }
  }

  std::vector<uint8_t> data_field;
  data_field.reserve(29);
  data_field.push_back(0xA2);  // 标识：定时设置

  for (size_t i = 0; i < 7; ++i) {
    const auto& task = normalized_tasks[i];
    data_field.push_back(static_cast<uint8_t>(task.weekday));
    data_field.push_back(static_cast<uint8_t>(task.hour));
    data_field.push_back(static_cast<uint8_t>(task.minute));
    data_field.push_back(static_cast<uint8_t>(task.run_count));
  }

  uint16_t robot_num = 0;
  try {
    if (!data_.robot_number.empty()) {
      robot_num = static_cast<uint16_t>(std::stoul(data_.robot_number));
    }
  } catch (...) {
    robot_num = 0;
  }

  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded =
      protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_num, frame_count, data_field);

  LOG(INFO) << "  定时设置编码后数据: " << Protocol::BytesToHexString(encoded);

  std::string base64_data = Protocol::BytesToBase64(encoded);
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  sequence_.fetch_add(1);
}

void Robot::SendParkingPositionRequest(uint8_t parking_position) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送停机位设置请求";
  LOG(INFO) << "  停机位: " << static_cast<int>(parking_position);

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  std::vector<uint8_t> data_field = {
      0xA3,  // 标识：停机位设置
      parking_position,
  };

  uint16_t robot_num = 0;
  try {
    if (!data_.robot_number.empty()) {
      robot_num = static_cast<uint16_t>(std::stoul(data_.robot_number));
    }
  } catch (...) {
    robot_num = 0;
  }

  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded =
      protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_num, frame_count, data_field);

  LOG(INFO) << "  停机位设置编码后数据: " << Protocol::BytesToHexString(encoded);

  std::string base64_data = Protocol::BytesToBase64(encoded);
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  sequence_.fetch_add(1);
}

void Robot::SendStartRequest() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送启动请求";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(0xF1) + 无参数
  std::vector<uint8_t> data_field = {
    0xF1  // 标识：启动请求
  };

  // 使用Protocol编码：控制码0x82（机器人主动发送请求）
  uint16_t robot_num = 0;
  try {
    if (!data_.robot_number.empty()) robot_num = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_num = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_num, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  // 转换为Base64
  std::string base64_data = Protocol::BytesToBase64(encoded);
  LOG(INFO) << "  Base64编码: " << base64_data;

  // 填入上行模板并发送
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  启动请求已加入发送队列";

  // 帧计数累加
  sequence_.fetch_add(1);
}

void Robot::SendTimeSyncRequest() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送校时请求";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(0xF2) + 无参数
  std::vector<uint8_t> data_field = {
    0xF2  // 标识：校时请求
  };

  // 使用Protocol编码：控制码0x82（机器人主动发送请求）
  uint16_t robot_num = 0;
  try {
    if (!data_.robot_number.empty()) robot_num = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_num = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_num, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  // 转换为Base64
  std::string base64_data = Protocol::BytesToBase64(encoded);
  LOG(INFO) << "  Base64编码: " << base64_data;

  // 填入上行模板并发送
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  校时请求已加入发送队列";

  // 帧计数累加
  sequence_.fetch_add(1);
}

void Robot::SendLoraAndCleanSettingsReport() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送Lora参数&清扫设置上报";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(0xE0) + Lora参数 + 清扫设置
  std::vector<uint8_t> data_field;

  // 标识符
  data_field.push_back(0xE0);

  // Lora参数 (3字节)
  data_field.push_back(static_cast<uint8_t>(data_.lora_params.power));      // 功率
  data_field.push_back(static_cast<uint8_t>(data_.lora_params.frequency));  // 频率
  data_field.push_back(static_cast<uint8_t>(data_.lora_params.rate));       // 速率

  // 机器人编号 (2字节) - 从data_.robot_number字符串转换为数字（若非数字则为0）
  uint16_t robot_num = 0;
  try {
    if (!data_.robot_number.empty()) robot_num = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_num = 0;
  }
  data_field.push_back(static_cast<uint8_t>(robot_num >> 8));   // 高字节
  data_field.push_back(static_cast<uint8_t>(robot_num & 0xFF)); // 低字节

  // 软件版本 (2字节) - 例如 "1.0" -> 0x01 0x00
  uint8_t major_version = 1;
  uint8_t minor_version = 0;
  if (!data_.software_version.empty()) {
    sscanf(data_.software_version.c_str(), "%hhu.%hhu", &major_version, &minor_version);
  }
  data_field.push_back(major_version);
  data_field.push_back(minor_version);

  // 启用/停用 (1字节)
  data_field.push_back(0x00);

  // 保留 (1字节) - 保留字段
  data_field.push_back(0x00);

  // 停机位 (1字节)
  data_field.push_back(static_cast<uint8_t>(data_.parking_position));

  // 白天防误扫开关 (1字节)
  data_field.push_back(data_.daytime_scan_protect ? 0x01 : 0x00);

  // 定时任务1-7 (每个4字节，共28字节)
  for (int i = 0; i < 7; i++) {
    const auto& task = data_.schedule_tasks[i];
    data_field.push_back(static_cast<uint8_t>(task.weekday));
    data_field.push_back(static_cast<uint8_t>(task.hour));
    data_field.push_back(static_cast<uint8_t>(task.minute));
    data_field.push_back(static_cast<uint8_t>(task.run_count));
  }


  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  // 使用Protocol编码：控制码0x82（机器人主动上报）
  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  // 转换为Base64
  std::string base64_data = Protocol::BytesToBase64(encoded);
  LOG(INFO) << "  Base64编码: " << base64_data;

  // 填入上行模板并发送
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  Lora参数&清扫设置上报已加入发送队列";

  // 帧计数累加
  sequence_.fetch_add(1);
}

void Robot::SendMotorParamsReport() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送电机参数主动上报";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(0xE1) + 电机参数(23字节) + 温度电压参数(15字节)
  std::vector<uint8_t> data_field;
  data_field.reserve(39);
  data_field.push_back(0xE1);  // 电机参数主动上报标识

  // 电机参数 (23字节)
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.walk_motor_speed));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.brush_motor_speed));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.windproof_motor_speed));

  data_field.push_back(static_cast<uint8_t>(data_.motor_params.walk_motor_max_current_ma >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.walk_motor_max_current_ma & 0xFF));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.brush_motor_max_current_ma >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.brush_motor_max_current_ma & 0xFF));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.windproof_motor_max_current_ma >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.windproof_motor_max_current_ma & 0xFF));

  data_field.push_back(static_cast<uint8_t>(data_.motor_params.walk_motor_warning_current_ma >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.walk_motor_warning_current_ma & 0xFF));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.brush_motor_warning_current_ma >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.brush_motor_warning_current_ma & 0xFF));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.windproof_motor_warning_current_ma >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.windproof_motor_warning_current_ma & 0xFF));

  data_field.push_back(static_cast<uint8_t>(data_.motor_params.walk_motor_mileage_m >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.walk_motor_mileage_m & 0xFF));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.brush_motor_timeout_s >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.brush_motor_timeout_s & 0xFF));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.windproof_motor_timeout_s >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.windproof_motor_timeout_s & 0xFF));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.reverse_time_s));
  data_field.push_back(static_cast<uint8_t>(data_.motor_params.protection_angle));

  // 温度电压参数 (15字节)
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.protection_current_ma >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.protection_current_ma & 0xFF));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.high_temp_threshold));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.low_temp_threshold));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.protection_temp));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.recovery_temp));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.protection_voltage));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.recovery_voltage));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.protection_battery_level));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.limit_run_battery_level));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.recovery_battery_level));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.board_protection_temp >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.board_protection_temp & 0xFF));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.board_recovery_temp >> 8));
  data_field.push_back(static_cast<uint8_t>(data_.temp_voltage_protection.board_recovery_temp & 0xFF));

  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  std::string base64_data = Protocol::BytesToBase64(encoded);
  LOG(INFO) << "  Base64编码: " << base64_data;

  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  电机参数主动上报已加入发送队列";

  sequence_.fetch_add(1);
}

void Robot::SendRobotDataReport() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送机器人数据上报";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(0xE4) + 机器人数据 (共46字节)
  std::vector<uint8_t> data_field = BuildRobotDataField(0xE4);

  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  // 使用Protocol编码：控制码0x82（机器人主动上报）
  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  // 转换为Base64
  std::string base64_data = Protocol::BytesToBase64(encoded);
  LOG(INFO) << "  Base64编码: " << base64_data;

  // 填入上行模板并发送
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  机器人数据上报已加入发送队列";

  // 帧计数累加
  sequence_.fetch_add(1);
}

void Robot::SendCleanRecordReport() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送清扫记录上报";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(0xE9) + 清扫记录 + 机器人编码信息 + 本地时间 + 主板温湿度
  std::vector<uint8_t> data_field;

  // 标识符
  data_field.push_back(0xE9);

  // 清扫记录5条，每条: 日(1) 时(1) 分(1) 清扫分钟数(2, 高字节先)
  // `clean_records` 已在构造时初始化为5个元素，直接读取
  for (size_t i = 0; i < 5; ++i) {
    const auto& rec = data_.clean_records[i];
    data_field.push_back(static_cast<uint8_t>(rec.day));
    data_field.push_back(static_cast<uint8_t>(rec.hour));
    data_field.push_back(static_cast<uint8_t>(rec.minute));
    uint16_t mins = static_cast<uint16_t>(rec.minutes);
    data_field.push_back(static_cast<uint8_t>(mins >> 8));
    data_field.push_back(static_cast<uint8_t>(mins & 0xFF));
    // 清扫结果 (1字节)
    data_field.push_back(static_cast<uint8_t>(rec.result));
    // 耗电量 (1字节)
    data_field.push_back(static_cast<uint8_t>(rec.energy));
  }

  // 机器人编码信息 6 字节：取 robot_id_ 的后6个字符（不足左填0）
  std::string dev_addr = robot_id_.length() >= 6 ? robot_id_.substr(robot_id_.length() - 6) : robot_id_;
  // 保证6字节
  while (dev_addr.size() < 6) dev_addr = std::string("\0") + dev_addr;
  for (size_t i = 0; i < 6; ++i) {
    char c = dev_addr[i];
    data_field.push_back(static_cast<uint8_t>(c));
  }

  // 在上报前更新时间字段与工作时长
  UpdateTimeFields();

  // 机器人本地时间 (年, 月, 日, 时, 分, 秒) - 年取两位
  int year = data_.local_time.year % 100;
  data_field.push_back(static_cast<uint8_t>(year));
  data_field.push_back(static_cast<uint8_t>(data_.local_time.month));
  data_field.push_back(static_cast<uint8_t>(data_.local_time.day));
  data_field.push_back(static_cast<uint8_t>(data_.local_time.hour));
  data_field.push_back(static_cast<uint8_t>(data_.local_time.minute));
  data_field.push_back(static_cast<uint8_t>(data_.local_time.second));

  // 主板温度 (2字节, 大端序)
  uint16_t board_temp16 = static_cast<uint16_t>(data_.board_temperature);
  data_field.push_back(static_cast<uint8_t>(board_temp16 >> 8));
  data_field.push_back(static_cast<uint8_t>(board_temp16 & 0xFF));

  // 主板湿度 (1字节)
  data_field.push_back(static_cast<uint8_t>(data_.board_humidity & 0xFF));

  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  // 使用Protocol编码：控制码0x82（机器人主动上报）
  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  // 转换为Base64并发送
  std::string base64_data = Protocol::BytesToBase64(encoded);
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  清扫记录上报已加入发送队列";

  // 帧计数累加
  sequence_.fetch_add(1);
}

void Robot::SendCurrentDataReport() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送电流数据上报 (0xE5)";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(0xE5) + 当前位置(2) + 当前方向(1)
  // + 电流上报间隔(2,大端) + 10组主/从机电流(各1字节交叉排列)
  std::vector<uint8_t> data_field;
  data_field.reserve(26);

  // 标识符
  data_field.push_back(0xE5);

  // 当前位置 (2字节，大端)
  uint16_t pos = static_cast<uint16_t>(data_.position);
  data_field.push_back(static_cast<uint8_t>(pos >> 8));
  data_field.push_back(static_cast<uint8_t>(pos & 0xFF));

  // 当前方向 (1字节)
  data_field.push_back(static_cast<uint8_t>(data_.direction));

  // 电流上报间隔（手动触发时填0）
  uint16_t interval = 0;
  data_field.push_back(static_cast<uint8_t>(interval >> 8));
  data_field.push_back(static_cast<uint8_t>(interval & 0xFF));

  // 10组主/从机电流，交叉排列：主机电流1, 从机电流1, ..., 主机电流10, 从机电流10
  // 每路各1字节
  // master_currents 和 slave_currents 均已初始化丶16个元素
  for (int i = 0; i < 10; ++i) {
    data_field.push_back(static_cast<uint8_t>(data_.master_currents[i]));
    data_field.push_back(static_cast<uint8_t>(data_.slave_currents[i]));
  }

  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  // 使用Protocol编码：控制砃0x82（机器人主动上报）
  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  // 转换为Base64并发送
  std::string base64_data = Protocol::BytesToBase64(encoded);
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  电流数据上报已加入发送队列";

  // 帧计数累加
  sequence_.fetch_add(1);
}

void Robot::SendScheduledNotRunReport() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送定时请求/未运行原因上报 (0xE6)";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(1) + 定时器编号(1) + 周(1) + 时(1) + 分(1) + 运行次数(1) + 原因(1) + 故障信息(4) = 11字节
  std::vector<uint8_t> data_field;
  data_field.reserve(11);

  // 标识符
  data_field.push_back(0xE6);

  // 定时器编号 (1字节, 1~7)
  uint8_t sid = data_.scheduled_not_run_id;
  data_field.push_back(sid);

  // 从 schedule_tasks 读取对应定时任务参数
  uint8_t weekday = 0, hour = 0, minute = 0;
  uint8_t run_count = 0;
  if (sid >= 1 && sid <= 7 && data_.schedule_tasks.size() >= static_cast<size_t>(sid)) {
    const auto& task = data_.schedule_tasks[sid - 1];
    weekday   = static_cast<uint8_t>(task.weekday);
    hour      = static_cast<uint8_t>(task.hour);
    minute    = static_cast<uint8_t>(task.minute);
    // 运行次数按协议：存储値已是实际所用，取低8位区间[0,127]
    int rc = task.run_count;
    run_count = (rc < 127) ? static_cast<uint8_t>(rc / 2) : static_cast<uint8_t>(rc);
  }
  data_field.push_back(weekday);
  data_field.push_back(hour);
  data_field.push_back(minute);
  data_field.push_back(run_count);

  // 未运行原因 (1字节)
  data_field.push_back(data_.scheduled_not_run_reason);

  // 故障信息 (4字节, 取 e6_alarm)
  uint32_t fa = data_.e6_alarm;
  data_field.push_back(static_cast<uint8_t>(fa >> 24));
  data_field.push_back(static_cast<uint8_t>(fa >> 16));
  data_field.push_back(static_cast<uint8_t>(fa >> 8));
  data_field.push_back(static_cast<uint8_t>(fa));

  LOG(INFO) << "  定时器编号:" << static_cast<int>(sid)
            << " 周" << static_cast<int>(weekday)
            << " " << static_cast<int>(hour) << ":" << static_cast<int>(minute)
            << " 运行次数:" << static_cast<int>(run_count)
            << " 原因:0x" << std::hex << static_cast<int>(data_.scheduled_not_run_reason);
  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  std::string base64_data = Protocol::BytesToBase64(encoded);
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  定时请求/未运行原因上报已加入发送队列";

  sequence_.fetch_add(1);
}

void Robot::SendNotStartedReport() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送未启动原因上报 (0xE7)";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(1) + 原因(1) + 故障信息(4) = 6字节
  std::vector<uint8_t> data_field;
  data_field.reserve(6);

  // 标识符
  data_field.push_back(0xE7);

  // 未启动原因 (1字节)
  data_field.push_back(data_.not_started_reason);

  // 故障信息 (4字节大端, 取 alarm_fa)
  uint32_t fa = data_.alarm_fa;
  data_.e7_alarm = fa;  // 快照当前故障信息
  data_field.push_back(static_cast<uint8_t>(fa >> 24));
  data_field.push_back(static_cast<uint8_t>(fa >> 16));
  data_field.push_back(static_cast<uint8_t>(fa >> 8));
  data_field.push_back(static_cast<uint8_t>(fa));

  LOG(INFO) << "  未启动原因:0x" << std::hex << static_cast<int>(data_.not_started_reason)
            << " 故障信息:0x" << std::hex << fa;
  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  std::string base64_data = Protocol::BytesToBase64(encoded);
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  未启动原因上报已加入发送队列";

  sequence_.fetch_add(1);
}

void Robot::SendStartupConfirmReport() {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送启动请求回复接收后确认 (0xE8)";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(1) + 定时器编号(1) = 2字节
  std::vector<uint8_t> data_field;
  data_field.reserve(2);

  // 标识符
  data_field.push_back(0xE8);

  // 定时器编号 (1字节)
  data_field.push_back(data_.startup_confirm_id);
  data_.e8_alarm = data_.alarm_fa;  // 快照当前故障信息

  LOG(INFO) << "  定时器编号:" << static_cast<int>(data_.startup_confirm_id);
  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  std::string base64_data = Protocol::BytesToBase64(encoded);
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  启动请求回复接收后确认已加入发送队列";

  sequence_.fetch_add(1);
}

void Robot::SendControlResponse(uint8_t control_identifier) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送控制响应 (标识符: 0x"
            << std::hex << static_cast<int>(control_identifier) << ")";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：标识(control_identifier) + 机器人数据 (共46字节)
  // 与SendRobotDataReport相同格式，只有标识符不同
  std::vector<uint8_t> data_field = BuildRobotDataField(control_identifier);

  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  // 使用Protocol编码：控制码0x82（机器人主动上报）
  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  // 转换为Base64
  std::string base64_data = Protocol::BytesToBase64(encoded);
  LOG(INFO) << "  Base64编码: " << base64_data;

  // 填入上行模板并发送
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  控制响应已加入发送队列";

  // 帧计数累加
  sequence_.fetch_add(1);
}

void Robot::SendRestartResponse(uint8_t control_identifier) {
  LOG(INFO) << "[Robot " << robot_id_ << "] 发送重启响应 (标识符: 0x"
            << std::hex << static_cast<int>(control_identifier) << ")";

  auto mqtt_manager = mqtt_manager_.lock();
  if (!mqtt_manager) {
    LOG(ERROR) << "  MQTT管理器未初始化";
    return;
  }

  // 构造数据域：仅包含标识符（1字节）
  std::vector<uint8_t> data_field = {control_identifier};

  LOG(INFO) << "  数据域长度: " << data_field.size() << " 字节";
  LOG(INFO) << "  数据域内容: " << Protocol::BytesToHexString(data_field);

  // 使用Protocol编码：控制码0x82（机器人主动上报）
  uint16_t robot_number = 0;
  try {
    if (!data_.robot_number.empty()) robot_number = static_cast<uint16_t>(std::stoul(data_.robot_number));
  } catch (...) {
    robot_number = 0;
  }
  uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
  std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_DOWNLINK, robot_number, frame_count, data_field);

  LOG(INFO) << "  编码后数据: " << Protocol::BytesToHexString(encoded);

  // 转换为Base64
  std::string base64_data = Protocol::BytesToBase64(encoded);
  LOG(INFO) << "  Base64编码: " << base64_data;

  // 填入上行模板并发送
  std::string payload = GenerateUplinkPayload(base64_data);
  mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);

  LOG(INFO) << "  重启响应已加入发送队列";

  // 帧计数累加
  sequence_.fetch_add(1);
}

// ── 控制指令动作函数 ────────────────────────────────────────────────────────

void Robot::ControlEnable() {
  data_.enabled = true;
  data_.alarm_fa |= static_cast<uint32_t>(AlarmFA::kDeviceEnabled);
  LOG(INFO) << "[Robot " << robot_id_ << "] 已启用";
}

void Robot::ControlDisable() {
  data_.enabled = false;
  data_.alarm_fa &= ~static_cast<uint32_t>(AlarmFA::kDeviceEnabled);
  move_direction_.store(0);
  LOG(INFO) << "[Robot " << robot_id_ << "] 已停用";
}

void Robot::ControlStart() {
  // 清除停止/前进/后退/已完成/已失败位
  const uint32_t clear_mask = static_cast<uint32_t>(AlarmFA::kStopped)
                            | static_cast<uint32_t>(AlarmFA::kForward)
                            | static_cast<uint32_t>(AlarmFA::kBackward)
                            | static_cast<uint32_t>(AlarmFA::kAutoCompleted)
                            | static_cast<uint32_t>(AlarmFA::kAutoFailed);
  data_.alarm_fa &= ~clear_mask;
  // 自动运行中、前进
  data_.alarm_fa |= static_cast<uint32_t>(AlarmFA::kAutoRunning)
                  | static_cast<uint32_t>(AlarmFA::kForward);
  move_direction_.store(1);
  LOG(INFO) << "[Robot " << robot_id_ << "] 启动清扫任务";
}

void Robot::ControlForward() {
  data_.alarm_fa &= ~static_cast<uint32_t>(AlarmFA::kBackward);
  data_.alarm_fa |= static_cast<uint32_t>(AlarmFA::kAutoRunning)
                  | static_cast<uint32_t>(AlarmFA::kForward);
  move_direction_.store(1);
  LOG(INFO) << "[Robot " << robot_id_ << "] 前进";
}

void Robot::ControlBackward() {
  data_.alarm_fa &= ~static_cast<uint32_t>(AlarmFA::kForward);
  data_.alarm_fa |= static_cast<uint32_t>(AlarmFA::kAutoRunning)
                  | static_cast<uint32_t>(AlarmFA::kBackward);
  move_direction_.store(-1);
  LOG(INFO) << "[Robot " << robot_id_ << "] 后退";
}

void Robot::ControlStop() {
  // 清除 Bit2-Bit8（kAutoManual~kBackward）
  const uint32_t clear_mask = static_cast<uint32_t>(AlarmFA::kAutoManual)
                            | static_cast<uint32_t>(AlarmFA::kStartFailed)
                            | static_cast<uint32_t>(AlarmFA::kAutoRunning)
                            | static_cast<uint32_t>(AlarmFA::kAutoCompleted)
                            | static_cast<uint32_t>(AlarmFA::kAutoFailed)
                            | static_cast<uint32_t>(AlarmFA::kForward)
                            | static_cast<uint32_t>(AlarmFA::kBackward);
  data_.alarm_fa &= ~clear_mask;
  data_.alarm_fa |= static_cast<uint32_t>(AlarmFA::kStopped);
  move_direction_.store(0);
  LOG(INFO) << "[Robot " << robot_id_ << "] 停止运行";
}

std::string Robot::SerializeDataSnapshot() const {
  // 在序列化前更新时间字段（允许修改内部缓存以保证返回的是最新时间）
  const_cast<Robot*>(this)->UpdateTimeFields();

  nlohmann::json d;
  d["alarm_fa"] = data_.alarm_fa;
  d["alarm_fb"] = data_.alarm_fb;
  d["alarm_fc"] = data_.alarm_fc;
  d["alarm_fd"] = data_.alarm_fd;
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

  // 清扫记录序列化
  nlohmann::json clean_arr = nlohmann::json::array();
  for (const auto& r : data_.clean_records) {
    clean_arr.push_back({
      {"day", r.day}, {"hour", r.hour}, {"minute", r.minute},
      {"minutes", r.minutes}, {"result", r.result}, {"energy", r.energy}
    });
  }
  d["clean_records"] = clean_arr;

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

  d["board_humidity"] = data_.board_humidity;
  d["scheduled_not_run_id"]     = data_.scheduled_not_run_id;
  d["scheduled_not_run_reason"] = data_.scheduled_not_run_reason;
  d["e6_alarm"]                  = data_.e6_alarm;
  d["not_started_reason"]       = data_.not_started_reason;
  d["e7_alarm"]                  = data_.e7_alarm;
  d["startup_confirm_id"]        = data_.startup_confirm_id;
  d["e8_alarm"]                  = data_.e8_alarm;

  // 数据模拟配置
  const auto& sc = data_.sim_config;
  d["sim_config"] = {
    {"enabled",              sc.enabled},
    {"main_current_random",   sc.main_current_random},
    {"main_current_min",      sc.main_current_min},
    {"main_current_max",      sc.main_current_max},
    {"main_current_fixed",    sc.main_current_fixed},
    {"slave_current_random",  sc.slave_current_random},
    {"slave_current_min",     sc.slave_current_min},
    {"slave_current_max",     sc.slave_current_max},
    {"slave_current_fixed",   sc.slave_current_fixed},
    {"solar_voltage_random",  sc.solar_voltage_random},
    {"solar_voltage_min",     sc.solar_voltage_min},
    {"solar_voltage_max",     sc.solar_voltage_max},
    {"solar_voltage_fixed",   sc.solar_voltage_fixed},
    {"solar_current_random",  sc.solar_current_random},
    {"solar_current_min",     sc.solar_current_min},
    {"solar_current_max",     sc.solar_current_max},
    {"solar_current_fixed",   sc.solar_current_fixed},
    {"board_temp_random",     sc.board_temp_random},
    {"board_temp_min",        sc.board_temp_min},
    {"board_temp_max",        sc.board_temp_max},
    {"board_temp_fixed",      sc.board_temp_fixed},
    {"battery_voltage_random",sc.battery_voltage_random},
    {"battery_voltage_min",   sc.battery_voltage_min},
    {"battery_voltage_max",   sc.battery_voltage_max},
    {"battery_voltage_fixed", sc.battery_voltage_fixed},
    {"battery_discharge_run", sc.battery_discharge_run},
    {"battery_discharge_stop",sc.battery_discharge_stop},
    {"battery_charge_rate",   sc.battery_charge_rate},
    {"battery_temp_random",   sc.battery_temp_random},
    {"battery_temp_min",      sc.battery_temp_min},
    {"battery_temp_max",      sc.battery_temp_max},
    {"battery_temp_fixed",    sc.battery_temp_fixed},
    {"alarm_sim", {
      {"enabled",       sc.alarm_sim.enabled},
      {"duration_min",  sc.alarm_sim.duration_min},
      {"duration_max",  sc.alarm_sim.duration_max},
      {"frequency",     sc.alarm_sim.frequency},
      {"fc_bits_mask",  sc.alarm_sim.fc_bits_mask}
    }}
  };

  return d.dump();
}

bool Robot::LoadDataSnapshot(const std::string& data_json) {
  if (data_json.empty()) {
    return false;
  }

  try {
    nlohmann::json d = nlohmann::json::parse(data_json);

    data_.alarm_fa = d.value("alarm_fa", data_.alarm_fa);
    data_.alarm_fb = d.value("alarm_fb", data_.alarm_fb);
    data_.alarm_fc = d.value("alarm_fc", data_.alarm_fc);
    data_.alarm_fd = d.value("alarm_fd", data_.alarm_fd);
    data_.main_motor_current = d.value("main_motor_current", data_.main_motor_current);
    data_.slave_motor_current = d.value("slave_motor_current", data_.slave_motor_current);
    data_.battery_voltage = d.value("battery_voltage", data_.battery_voltage);
    data_.battery_current = d.value("battery_current", data_.battery_current);
    data_.battery_status = d.value("battery_status", data_.battery_status);
    data_.battery_level = d.value("battery_level", data_.battery_level);
    data_.battery_temperature = d.value("battery_temperature", data_.battery_temperature);
    data_.position_info = d.value("position_info", data_.position_info);
    data_.working_duration = d.value("working_duration", data_.working_duration);
    data_.total_run_count = d.value("total_run_count", data_.total_run_count);
    data_.current_lap_count = d.value("current_lap_count", data_.current_lap_count);
    data_.solar_voltage = d.value("solar_voltage", data_.solar_voltage);
    data_.solar_current = d.value("solar_current", data_.solar_current);
    data_.board_temperature = d.value("board_temperature", data_.board_temperature);

    if (d.contains("current_timestamp") && d["current_timestamp"].is_object()) {
      const auto& ts = d["current_timestamp"];
      data_.current_timestamp.hour = ts.value("hour", data_.current_timestamp.hour);
      data_.current_timestamp.minute = ts.value("minute", data_.current_timestamp.minute);
      data_.current_timestamp.second = ts.value("second", data_.current_timestamp.second);
    }

    if (d.contains("lora_params") && d["lora_params"].is_object()) {
      const auto& lora = d["lora_params"];
      data_.lora_params.power = lora.value("power", data_.lora_params.power);
      data_.lora_params.frequency = lora.value("frequency", data_.lora_params.frequency);
      data_.lora_params.rate = lora.value("rate", data_.lora_params.rate);
    }

    data_.robot_number = d.value("robot_number", data_.robot_number);
    data_.software_version = d.value("software_version", data_.software_version);
    data_.parking_position = d.value("parking_position", data_.parking_position);
    data_.daytime_scan_protect = d.value("daytime_scan_protect", data_.daytime_scan_protect);
    data_.enabled = d.value("enabled", data_.enabled);

    if (d.contains("schedule_tasks") && d["schedule_tasks"].is_array()) {
      data_.schedule_tasks.clear();
      for (const auto& item : d["schedule_tasks"]) {
        ScheduleTask task;
        task.weekday = item.value("weekday", 0);
        task.hour = item.value("hour", 0);
        task.minute = item.value("minute", 0);
        task.run_count = item.value("run_count", 0);
        data_.schedule_tasks.push_back(task);
      }
    }

    if (d.contains("clean_records") && d["clean_records"].is_array()) {
      data_.clean_records.clear();
      for (const auto& item : d["clean_records"]) {
        RobotData::CleanRecord record;
        record.day = static_cast<uint8_t>(item.value("day", 0));
        record.hour = static_cast<uint8_t>(item.value("hour", 0));
        record.minute = static_cast<uint8_t>(item.value("minute", 0));
        record.minutes = static_cast<uint16_t>(item.value("minutes", 0));
        record.result = static_cast<uint8_t>(item.value("result", 0));
        record.energy = static_cast<uint8_t>(item.value("energy", 0));
        data_.clean_records.push_back(record);
      }
    }

    if (d.contains("motor_params") && d["motor_params"].is_object()) {
      const auto& mp = d["motor_params"];
      data_.motor_params.walk_motor_speed = mp.value("walk_motor_speed", data_.motor_params.walk_motor_speed);
      data_.motor_params.brush_motor_speed = mp.value("brush_motor_speed", data_.motor_params.brush_motor_speed);
      data_.motor_params.windproof_motor_speed = mp.value("windproof_motor_speed", data_.motor_params.windproof_motor_speed);
      data_.motor_params.walk_motor_max_current_ma = mp.value("walk_motor_max_current_ma", data_.motor_params.walk_motor_max_current_ma);
      data_.motor_params.brush_motor_max_current_ma = mp.value("brush_motor_max_current_ma", data_.motor_params.brush_motor_max_current_ma);
      data_.motor_params.windproof_motor_max_current_ma = mp.value("windproof_motor_max_current_ma", data_.motor_params.windproof_motor_max_current_ma);
      data_.motor_params.walk_motor_warning_current_ma = mp.value("walk_motor_warning_current_ma", data_.motor_params.walk_motor_warning_current_ma);
      data_.motor_params.brush_motor_warning_current_ma = mp.value("brush_motor_warning_current_ma", data_.motor_params.brush_motor_warning_current_ma);
      data_.motor_params.windproof_motor_warning_current_ma = mp.value("windproof_motor_warning_current_ma", data_.motor_params.windproof_motor_warning_current_ma);
      data_.motor_params.walk_motor_mileage_m = mp.value("walk_motor_mileage_m", data_.motor_params.walk_motor_mileage_m);
      data_.motor_params.brush_motor_timeout_s = mp.value("brush_motor_timeout_s", data_.motor_params.brush_motor_timeout_s);
      data_.motor_params.windproof_motor_timeout_s = mp.value("windproof_motor_timeout_s", data_.motor_params.windproof_motor_timeout_s);
      data_.motor_params.reverse_time_s = mp.value("reverse_time_s", data_.motor_params.reverse_time_s);
      data_.motor_params.protection_angle = mp.value("protection_angle", data_.motor_params.protection_angle);
    }

    if (d.contains("temp_voltage_protection") && d["temp_voltage_protection"].is_object()) {
      const auto& tv = d["temp_voltage_protection"];
      data_.temp_voltage_protection.protection_current_ma = tv.value("protection_current_ma", data_.temp_voltage_protection.protection_current_ma);
      data_.temp_voltage_protection.high_temp_threshold = tv.value("high_temp_threshold", data_.temp_voltage_protection.high_temp_threshold);
      data_.temp_voltage_protection.low_temp_threshold = tv.value("low_temp_threshold", data_.temp_voltage_protection.low_temp_threshold);
      data_.temp_voltage_protection.protection_temp = tv.value("protection_temp", data_.temp_voltage_protection.protection_temp);
      data_.temp_voltage_protection.recovery_temp = tv.value("recovery_temp", data_.temp_voltage_protection.recovery_temp);
      data_.temp_voltage_protection.protection_voltage = tv.value("protection_voltage", data_.temp_voltage_protection.protection_voltage);
      data_.temp_voltage_protection.recovery_voltage = tv.value("recovery_voltage", data_.temp_voltage_protection.recovery_voltage);
      data_.temp_voltage_protection.protection_battery_level = tv.value("protection_battery_level", data_.temp_voltage_protection.protection_battery_level);
      data_.temp_voltage_protection.limit_run_battery_level = tv.value("limit_run_battery_level", data_.temp_voltage_protection.limit_run_battery_level);
      data_.temp_voltage_protection.recovery_battery_level = tv.value("recovery_battery_level", data_.temp_voltage_protection.recovery_battery_level);
      data_.temp_voltage_protection.board_protection_temp = tv.value("board_protection_temp", data_.temp_voltage_protection.board_protection_temp);
      data_.temp_voltage_protection.board_recovery_temp = tv.value("board_recovery_temp", data_.temp_voltage_protection.board_recovery_temp);
    }

    if (d.contains("local_time") && d["local_time"].is_object()) {
      const auto& lt = d["local_time"];
      data_.local_time.year = lt.value("year", data_.local_time.year);
      data_.local_time.month = lt.value("month", data_.local_time.month);
      data_.local_time.day = lt.value("day", data_.local_time.day);
      data_.local_time.hour = lt.value("hour", data_.local_time.hour);
      data_.local_time.minute = lt.value("minute", data_.local_time.minute);
      data_.local_time.second = lt.value("second", data_.local_time.second);
      data_.local_time.weekday = lt.value("weekday", data_.local_time.weekday);
    }

    if (d.contains("environment_info") && d["environment_info"].is_object()) {
      const auto& env = d["environment_info"];
      data_.environment_info.sensor_temperature = env.value("sensor_temperature", data_.environment_info.sensor_temperature);
      data_.environment_info.sensor_humidity = env.value("sensor_humidity", data_.environment_info.sensor_humidity);
      data_.environment_info.ambient_temperature = env.value("ambient_temperature", data_.environment_info.ambient_temperature);
      data_.environment_info.day_night_status = env.value("day_night_status", data_.environment_info.day_night_status);
    }

    if (d.contains("master_currents") && d["master_currents"].is_array()) {
      data_.master_currents.clear();
      for (const auto& value : d["master_currents"]) {
        data_.master_currents.push_back(value.get<int>());
      }
    }

    if (d.contains("slave_currents") && d["slave_currents"].is_array()) {
      data_.slave_currents.clear();
      for (const auto& value : d["slave_currents"]) {
        data_.slave_currents.push_back(value.get<int>());
      }
    }

    data_.position = d.value("position", data_.position);
    data_.direction = d.value("direction", data_.direction);
    data_.module_eui = d.value("module_eui", data_.module_eui);
    data_.domestic_foreign_flag = d.value("domestic_foreign_flag", data_.domestic_foreign_flag);
    data_.country_code = d.value("country_code", data_.country_code);
    data_.region_code = d.value("region_code", data_.region_code);
    data_.project_code = d.value("project_code", data_.project_code);
    data_.board_humidity = d.value("board_humidity", data_.board_humidity);
    data_.scheduled_not_run_id     = d.value("scheduled_not_run_id",     data_.scheduled_not_run_id);
    data_.scheduled_not_run_reason = d.value("scheduled_not_run_reason", data_.scheduled_not_run_reason);
    data_.e6_alarm                  = d.value("e6_alarm",                  data_.e6_alarm);
    data_.not_started_reason       = d.value("not_started_reason",       data_.not_started_reason);
    data_.e7_alarm                  = d.value("e7_alarm",                  data_.e7_alarm);
    data_.startup_confirm_id        = d.value("startup_confirm_id",        data_.startup_confirm_id);
    data_.e8_alarm                  = d.value("e8_alarm",                  data_.e8_alarm);

    if (d.contains("sim_config") && d["sim_config"].is_object()) {
      const auto& sc = d["sim_config"];
      auto& s = data_.sim_config;
      s.enabled                = sc.value("enabled",                s.enabled);
      s.main_current_random    = sc.value("main_current_random",    s.main_current_random);
      s.main_current_min       = sc.value("main_current_min",       s.main_current_min);
      s.main_current_max       = sc.value("main_current_max",       s.main_current_max);
      s.main_current_fixed     = sc.value("main_current_fixed",     s.main_current_fixed);
      s.slave_current_random   = sc.value("slave_current_random",   s.slave_current_random);
      s.slave_current_min      = sc.value("slave_current_min",      s.slave_current_min);
      s.slave_current_max      = sc.value("slave_current_max",      s.slave_current_max);
      s.slave_current_fixed    = sc.value("slave_current_fixed",    s.slave_current_fixed);
      s.solar_voltage_random   = sc.value("solar_voltage_random",   s.solar_voltage_random);
      s.solar_voltage_min      = sc.value("solar_voltage_min",      s.solar_voltage_min);
      s.solar_voltage_max      = sc.value("solar_voltage_max",      s.solar_voltage_max);
      s.solar_voltage_fixed    = sc.value("solar_voltage_fixed",    s.solar_voltage_fixed);
      s.solar_current_random   = sc.value("solar_current_random",   s.solar_current_random);
      s.solar_current_min      = sc.value("solar_current_min",      s.solar_current_min);
      s.solar_current_max      = sc.value("solar_current_max",      s.solar_current_max);
      s.solar_current_fixed    = sc.value("solar_current_fixed",    s.solar_current_fixed);
      s.board_temp_random      = sc.value("board_temp_random",      s.board_temp_random);
      s.board_temp_min         = sc.value("board_temp_min",         s.board_temp_min);
      s.board_temp_max         = sc.value("board_temp_max",         s.board_temp_max);
      s.board_temp_fixed       = sc.value("board_temp_fixed",       s.board_temp_fixed);
      s.battery_voltage_random = sc.value("battery_voltage_random", s.battery_voltage_random);
      s.battery_voltage_min    = sc.value("battery_voltage_min",    s.battery_voltage_min);
      s.battery_voltage_max    = sc.value("battery_voltage_max",    s.battery_voltage_max);
      s.battery_voltage_fixed  = sc.value("battery_voltage_fixed",  s.battery_voltage_fixed);
      s.battery_discharge_run  = sc.value("battery_discharge_run",  s.battery_discharge_run);
      s.battery_discharge_stop = sc.value("battery_discharge_stop", s.battery_discharge_stop);
      s.battery_charge_rate    = sc.value("battery_charge_rate",    s.battery_charge_rate);
      s.battery_temp_random    = sc.value("battery_temp_random",    s.battery_temp_random);
      s.battery_temp_min       = sc.value("battery_temp_min",       s.battery_temp_min);
      s.battery_temp_max       = sc.value("battery_temp_max",       s.battery_temp_max);
      s.battery_temp_fixed     = sc.value("battery_temp_fixed",     s.battery_temp_fixed);
      if (sc.contains("alarm_sim") && sc["alarm_sim"].is_object()) {
        const auto& aj = sc["alarm_sim"];
        auto& a = s.alarm_sim;
        a.enabled      = aj.value("enabled",      a.enabled);
        a.duration_min = aj.value("duration_min", a.duration_min);
        a.duration_max = aj.value("duration_max", a.duration_max);
        a.frequency    = aj.value("frequency",    a.frequency);
        a.fc_bits_mask = aj.value("fc_bits_mask", a.fc_bits_mask);
      }
    }

    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "[Robot " << robot_id_ << "] 读取数据快照失败: " << e.what();
    return false;
  }
}

std::string Robot::GetLastData() const {
  nlohmann::json j;

  j["robot_id"] = robot_id_;
  j["publish_topic"] = publish_topic_;
  j["subscribe_topic"] = subscribe_topic_;
  j["sequence"] = sequence_.load();
  j["robot_data_report_interval_s"] = robot_data_report_interval_s_;
  j["motor_params_report_interval_s"] = motor_params_report_interval_s_;
  j["lora_clean_report_interval_s"] = lora_clean_report_interval_s_;
  j["running"] = IsRunning();

  try {
    j["data"] = nlohmann::json::parse(SerializeDataSnapshot());
  } catch (...) {
    j["data"] = nlohmann::json::object();
  }

  return j.dump();
}
