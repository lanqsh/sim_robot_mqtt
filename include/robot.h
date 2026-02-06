#ifndef ROBOT_H_
#define ROBOT_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include "protocol.h"

// 指令类型
enum class MessageDirection {
  kUplink,   // 上行
  kDownlink  // 下行
};

enum class UplinkType {
  kReport,  // 上报
  kRequest  // 请求
};

enum class DownlinkType {
  kControl,  // 控制
  kQuery     // 查询
};

// LoRa参数
struct LoraParams {
  int power = 0;      // 功率
  int frequency = 0;  // 频率
  int rate = 0;       // 速率
};

// 定时任务
struct ScheduleTask {
  int weekday = 0;    // 星期 (0-6)
  int hour = 0;       // 时
  int minute = 0;     // 分
  int run_count = 0;  // 运行次数
};

// 电机参数
struct MotorParams {
  int walk_motor_speed = 0;                    // 行走电机速率
  int brush_motor_speed = 0;                   // 毛刷电机速率
  int windproof_motor_speed = 0;               // 防风电机速率
  int walk_motor_max_current_ma = 0;           // 行走电机上限电流停机值（mA）
  int brush_motor_max_current_ma = 0;          // 毛刷电机上限电流停机值（mA）
  int windproof_motor_max_current_ma = 0;      // 防风电机上限电流停机值（mA）
  int walk_motor_warning_current_ma = 0;       // 行走电机上限电流预警值（mA）
  int brush_motor_warning_current_ma = 0;      // 毛刷电机上限电流预警值（mA）
  int windproof_motor_warning_current_ma = 0;  // 防风电机上限电流预警值（mA）
  int walk_motor_mileage_m = 0;                // 行走电机运行里程（米）
  int brush_motor_timeout_s = 0;               // 毛刷电机运行超时时间（秒）
  int windproof_motor_timeout_s = 0;           // 防风电机运行超时时间（秒）
  int reverse_time_s = 0;                      // 运行反转时间（秒）
  int protection_angle = 0;                    // 保护角度
};

// 温度电压保护参数
struct TempVoltageProtection {
  int protection_current_ma = 0;     // 保护电流（mA）
  int high_temp_threshold = 0;       // 高温阈值
  int low_temp_threshold = 0;        // 低温阈值
  int protection_temp = 0;           // 保护温度
  int recovery_temp = 0;             // 恢复温度
  int protection_voltage = 0;        // 保护电压
  int recovery_voltage = 0;          // 恢复电压
  int protection_battery_level = 0;  // 保护电量
  int limit_run_battery_level = 0;   // 限制运行电量
  int recovery_battery_level = 0;    // 恢复电量
  int board_protection_temp = 0;     // 主板保护温度
  int board_recovery_temp = 0;       // 主板恢复温度
};

// 机器人本地时间
struct RobotLocalTime {
  int year = 0;     // 年
  int month = 0;    // 月
  int day = 0;      // 日
  int hour = 0;     // 时
  int minute = 0;   // 分
  int second = 0;   // 秒
  int weekday = 0;  // 星期
};

// 环境信息
struct EnvironmentInfo {
  float sensor_temperature = 0.0f;   // 温湿度传感器温度
  float sensor_humidity = 0.0f;      // 温湿度传感器湿度
  float ambient_temperature = 0.0f;  // 环境温度
  int day_night_status = 0;          // 白夜状态
};

// 机器人数据结构
struct RobotData {
  // 告警信息
  uint32_t alarm_fa = 0;  // FA告警 (4字节)
  uint16_t alarm_fb = 0;  // FB告警 (2字节)
  uint32_t alarm_fc = 0;  // FC告警 (4字节)
  uint16_t alarm_fd = 0;  // FD告警 (2字节)

  // 电流电压
  int main_motor_current = 0;   // 主电机电流（100mA）
  int slave_motor_current = 0;  // 从电机电流（100mA）
  int battery_voltage = 0;      // 电池电压（100mV）
  int battery_current = 0;      // 电池电流（100mA）

  // 电池信息
  int battery_status = 0;       // 电池状态
  int battery_level = 100;      // 电池电量
  int battery_temperature = 0;  // 电池温度

  // 位置和运行信息
  std::string position_info;    // 位置信息
  int working_duration = 0;     // 工作时长
  int total_run_count = 0;      // 累计运行次数
  int current_lap_count = 0;    // 当前运行圈数

  // 光伏信息
  int solar_voltage = 0;  // 光伏板输出电压（100mV）
  int solar_current = 0;  // 光伏板输出电流（100mA）

  // 时间戳
  struct {
    int hour = 0;    // 时
    int minute = 0;  // 分
    int second = 0;  // 秒
  } current_timestamp;  // 当前变化时间戳

  // 温度
  int board_temperature = 0;  // 主板温度

  // 配置参数
  LoraParams lora_params{};                   // LoRa参数
  std::string robot_number;                   // 机器人编号
  std::string software_version;               // 软件版本
  int parking_position = 0;                   // 停机位
  bool daytime_scan_protect = false;          // 白天防误扫开关
  std::vector<ScheduleTask> schedule_tasks;   // 定时任务
  bool enabled = true;                        // 启用/停用

  // 电机和保护参数
  MotorParams motor_params{};
  TempVoltageProtection temp_voltage_protection{};

  // 时间和环境
  RobotLocalTime local_time{};
  EnvironmentInfo environment_info{};

  // 数组数据
  std::vector<int> master_currents;  // 主机电流
  std::vector<int> slave_currents;   // 从机电流
  int position = 0;                  // 位置
  int direction = 0;                 // 方向

  // 清扫记录（最多5条）
  struct CleanRecord {
    uint8_t day = 0;       // 日
    uint8_t hour = 0;      // 时
    uint8_t minute = 0;    // 分
    uint16_t minutes = 0;  // 清扫分钟数（uint16_t，高低字节）
    uint8_t result = 0;    // 清扫结果 (1字节)
    uint8_t energy = 0;    // 耗电量 (1字节, 单位按协议定义)
  };
  std::vector<CleanRecord> clean_records; // 最多5条清扫记录

  int board_humidity = 0; // 主板湿度（％，可用1字节或按需扩展）
  // 设备标识
  std::string module_eui;            // 模组EUI
  int domestic_foreign_flag = 0;     // 国内外版本标识
  std::string country_code;          // 国家代码
  std::string region_code;           // 地区代码
  std::string project_code;          // 项目代码
};

// 前向声明
class MqttManager;

class Robot {
 public:
  explicit Robot(const std::string& robot_id);
  ~Robot();

  // 获取机器人ID
  std::string GetId() const { return robot_id_; }

  // 设置主题
  void SetTopics(const std::string& publish_topic,
                 const std::string& subscribe_topic);

  // 设置MQTT管理器（用于发送消息）
  void SetMqttManager(std::shared_ptr<MqttManager> manager);

  // 设置上报间隔（秒）
  void SetReportInterval(int interval_seconds);

  // 启动定时上报
  void StartReport();

  // 停止定时上报
  void StopReport();

  // 获取发布主题
  std::string GetPublishTopic() const { return publish_topic_; }

  // 获取订阅主题
  std::string GetSubscribeTopic() const { return subscribe_topic_; }

  // 生成上行数据（使用模板）
  std::string GenerateUplinkPayload(const std::string& data);

  // 处理接收到的订阅消息
  void HandleMessage(const std::string& data);

  // 获取和设置机器人数据
  RobotData& GetData() { return data_; }
  const RobotData& GetData() const { return data_; }

  // HTTP API支持
  bool IsRunning() const { return !stop_report_; }
  std::string GetLastData() const;  // 获取最后一次上报的数据（JSON格式）

  // 请求类指令
  void SendScheduleStartRequest(uint8_t schedule_id, uint8_t weekday,
                                uint8_t hour, uint8_t minute, uint8_t run_count);
  void SendStartRequest();  // 启动请求
  void SendTimeSyncRequest();  // 校时请求

  // 上报类指令
  void SendLoraAndCleanSettingsReport();  // Lora参数&清扫设置上报
  void SendRobotDataReport();             // 机器人数据上报
  void SendCleanRecordReport();           // 清扫记录上报 (标识符 0xE9)

 private:
  std::string robot_id_;                   // 机器人ID
  std::string publish_topic_;              // 发布主题
  std::string subscribe_topic_;            // 订阅主题
  std::atomic<int> sequence_{0};           // 序列号
  RobotData data_;                         // 机器人完整数据

  // MQTT管理器（用于发送消息）
  std::weak_ptr<MqttManager> mqtt_manager_;

  // 上报线程相关
  std::thread report_thread_;              // 上报线程
  std::atomic<bool> stop_report_{false};   // 停止上报标志
  int report_interval_seconds_{10};        // 上报间隔（秒）
  Protocol protocol_;                      // 协议编解码器

  // 上报线程函数
  void ReportThreadFunc();

  // 读取上行数据模板
  static std::string LoadUplinkTemplate();
  static std::string uplink_template_;  // 静态模板缓存
};

#endif  // ROBOT_H_
