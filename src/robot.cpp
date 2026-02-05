#include "robot.h"

#include <glog/logging.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iomanip>
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
  StopReport();
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
              LOG(INFO) << "      - 大风保护: " << ((protection_info & 0x01) ? "开启" : "关闭");
              LOG(INFO) << "      - 湿度保护: " << ((protection_info & 0x02) ? "开启" : "关闭");
              LOG(INFO) << "      - 支架保护: " << ((protection_info & 0x04) ? "开启" : "关闭");
              LOG(INFO) << "      - 环境温度保护: " << ((protection_info & 0x08) ? "开启" : "关闭");
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
              LOG(INFO) << "      - 大风保护: " << ((protection_info & 0x01) ? "开启" : "关闭");
              LOG(INFO) << "      - 湿度保护: " << ((protection_info & 0x02) ? "开启" : "关闭");
              LOG(INFO) << "      - 支架保护: " << ((protection_info & 0x04) ? "开启" : "关闭");
              LOG(INFO) << "      - 环境温度保护: " << ((protection_info & 0x08) ? "开启" : "关闭");
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
              LOG(INFO) << "      - 大风保护: " << ((protection_info & 0x01) ? "开启" : "关闭");
              LOG(INFO) << "      - 湿度保护: " << ((protection_info & 0x02) ? "开启" : "关闭");
              LOG(INFO) << "      - 支架保护: " << ((protection_info & 0x04) ? "开启" : "关闭");
              LOG(INFO) << "      - 环境温度保护: " << ((protection_info & 0x08) ? "开启" : "关闭");
            } else {
              LOG(ERROR) << "    校时请求回复数据长度不足";
            }
            break;
          }

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
  report_interval_seconds_ = interval_seconds;
  LOG(INFO) << "[Robot " << robot_id_ << "] 设置上报间隔为 " << interval_seconds << " 秒";
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
      // 构造数据域（标识 + 参数）
      // 示例：标识0xA4（LoRa参数设置），参数：0x14 0x50 0x01
      std::vector<uint8_t> data_field = {0xA4, 0x14, 0x50, 0x01};

      // 使用Protocol编码：控制码0x41（上行），编号（取robot_number或序号），帧计数（sequence_累加）
      uint16_t robot_num = 2;  // TODO: 从data_.robot_number解析或使用配置的序号
      uint8_t frame_count = static_cast<uint8_t>(sequence_.load() & 0xFF);
      std::vector<uint8_t> encoded = protocol_.Encode(CONTROL_CODE_UPLINK, robot_num, frame_count, data_field);

      // 转换为Base64
      std::string base64_data = Protocol::BytesToBase64(encoded);

      // 填入上行模板
      std::string payload = GenerateUplinkPayload(base64_data);

      // 将消息加入发送队列
      mqtt_manager->EnqueueMessage(publish_topic_, payload, 1);
      LOG(INFO) << "[Robot " << robot_id_ << "] 上报数据已加入队列，Base64: " << base64_data;

      // 帧计数累加
      sequence_.fetch_add(1);
    }
  }

  LOG(INFO) << "[Robot " << robot_id_ << "] 上报线程已停止";
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
  uint16_t robot_num = 2;  // TODO: 使用实际的robot_number
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
  uint16_t robot_num = 2;  // TODO: 使用实际的robot_number
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
  uint16_t robot_num = 2;  // TODO: 使用实际的robot_number
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
