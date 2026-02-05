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
  j["机器人ID"] = robot_id_;
  j["发布主题"] = publish_topic_;
  j["订阅主题"] = subscribe_topic_;
  j["序列号"] = sequence_.load();
  j["上报间隔(秒)"] = report_interval_seconds_;
  j["运行状态"] = IsRunning();

  // RobotData 全量序列化
  nlohmann::json d;
  d["主电机电流"] = data_.main_motor_current;
  d["从电机电流"] = data_.slave_motor_current;
  d["电池电压"] = data_.battery_voltage;
  d["电池电流"] = data_.battery_current;
  d["电池状态"] = data_.battery_status;
  d["电池电量"] = data_.battery_level;
  d["电池温度"] = data_.battery_temperature;
  d["位置信息"] = data_.position_info;
  d["工作时长"] = data_.working_duration;
  d["累计运行次数"] = data_.total_run_count;
  d["当前圈数"] = data_.current_lap_count;
  d["光伏电压"] = data_.solar_voltage;
  d["光伏电流"] = data_.solar_current;

  // timestamp
  d["当前时间戳"] = {
    {"时", data_.current_timestamp.hour},
    {"分", data_.current_timestamp.minute},
    {"秒", data_.current_timestamp.second}
  };

  d["主板温度"] = data_.board_temperature;

  // 配置参数
  d["LoRa参数"] = {
    {"功率", data_.lora_params.power},
    {"频率", data_.lora_params.frequency},
    {"速率", data_.lora_params.rate}
  };

  d["机器人编号"] = data_.robot_number;
  d["软件版本"] = data_.software_version;
  d["停机位"] = data_.parking_position;
  d["白天防误扫"] = data_.daytime_scan_protect;

  // schedule tasks
  nlohmann::json tasks = nlohmann::json::array();
  for (const auto& t : data_.schedule_tasks) {
    tasks.push_back({
      {"星期", t.weekday}, {"时", t.hour}, {"分", t.minute}, {"运行次数", t.run_count}
    });
  }
  d["定时任务"] = tasks;

  d["是否启用"] = data_.enabled;

  // 电机参数
  d["电机参数"] = {
    {"行走电机速率", data_.motor_params.walk_motor_speed},
    {"毛刷电机速率", data_.motor_params.brush_motor_speed},
    {"防风电机速率", data_.motor_params.windproof_motor_speed},
    {"行走电机上限电流停机值(mA)", data_.motor_params.walk_motor_max_current_ma},
    {"毛刷电机上限电流停机值(mA)", data_.motor_params.brush_motor_max_current_ma},
    {"防风电机上限电流停机值(mA)", data_.motor_params.windproof_motor_max_current_ma},
    {"行走电机预警电流(mA)", data_.motor_params.walk_motor_warning_current_ma},
    {"毛刷电机预警电流(mA)", data_.motor_params.brush_motor_warning_current_ma},
    {"防风电机预警电流(mA)", data_.motor_params.windproof_motor_warning_current_ma},
    {"行走电机运行里程(m)", data_.motor_params.walk_motor_mileage_m},
    {"毛刷电机超时(s)", data_.motor_params.brush_motor_timeout_s},
    {"防风电机超时(s)", data_.motor_params.windproof_motor_timeout_s},
    {"反转时间(s)", data_.motor_params.reverse_time_s},
    {"保护角度", data_.motor_params.protection_angle}
  };

  // 保护参数
  d["温压保护参数"] = {
    {"保护电流(mA)", data_.temp_voltage_protection.protection_current_ma},
    {"高温阈值", data_.temp_voltage_protection.high_temp_threshold},
    {"低温阈值", data_.temp_voltage_protection.low_temp_threshold},
    {"保护温度", data_.temp_voltage_protection.protection_temp},
    {"恢复温度", data_.temp_voltage_protection.recovery_temp},
    {"保护电压", data_.temp_voltage_protection.protection_voltage},
    {"恢复电压", data_.temp_voltage_protection.recovery_voltage},
    {"保护电量", data_.temp_voltage_protection.protection_battery_level},
    {"限制运行电量", data_.temp_voltage_protection.limit_run_battery_level},
    {"恢复电量", data_.temp_voltage_protection.recovery_battery_level},
    {"主板保护温度", data_.temp_voltage_protection.board_protection_temp},
    {"主板恢复温度", data_.temp_voltage_protection.board_recovery_temp}
  };

  // 本地时间
  d["本地时间"] = {
    {"年", data_.local_time.year}, {"月", data_.local_time.month}, {"日", data_.local_time.day},
    {"时", data_.local_time.hour}, {"分", data_.local_time.minute}, {"秒", data_.local_time.second},
    {"星期", data_.local_time.weekday}
  };

  // 环境信息
  d["环境信息"] = {
    {"传感器温度", data_.environment_info.sensor_temperature},
    {"传感器湿度", data_.environment_info.sensor_humidity},
    {"环境温度", data_.environment_info.ambient_temperature},
    {"白夜状态", data_.environment_info.day_night_status}
  };

  // 数组数据
  d["主机电流"] = data_.master_currents;
  d["从机电流"] = data_.slave_currents;
  d["位置"] = data_.position;
  d["方向"] = data_.direction;

  // 设备标识
  d["模组EUI"] = data_.module_eui;
  d["国内/国外版本"] = data_.domestic_foreign_flag;
  d["国家代码"] = data_.country_code;
  d["地区代码"] = data_.region_code;
  d["项目代码"] = data_.project_code;

  j["数据"] = d;

  return j.dump();
}
