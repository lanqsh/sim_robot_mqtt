#ifndef ROBOT_H_
#define ROBOT_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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
  int power;      // 功率
  int frequency;  // 频率
  int rate;       // 速率
};

// 定时任务
struct ScheduleTask {
  int weekday;    // 星期 (0-6)
  int hour;       // 时
  int minute;     // 分
  int run_count;  // 运行次数
};

// 电机参数
struct MotorParams {
  int walk_motor_speed;                    // 行走电机速率
  int brush_motor_speed;                   // 毛刷电机速率
  int windproof_motor_speed;               // 防风电机速率
  int walk_motor_max_current_ma;           // 行走电机上限电流停机值（mA）
  int brush_motor_max_current_ma;          // 毛刷电机上限电流停机值（mA）
  int windproof_motor_max_current_ma;      // 防风电机上限电流停机值（mA）
  int walk_motor_warning_current_ma;       // 行走电机上限电流预警值（mA）
  int brush_motor_warning_current_ma;      // 毛刷电机上限电流预警值（mA）
  int windproof_motor_warning_current_ma;  // 防风电机上限电流预警值（mA）
  int walk_motor_mileage_m;                // 行走电机运行里程（米）
  int brush_motor_timeout_s;               // 毛刷电机运行超时时间（秒）
  int windproof_motor_timeout_s;           // 防风电机运行超时时间（秒）
  int reverse_time_s;                      // 运行反转时间（秒）
  int protection_angle;                    // 保护角度
};

// 温度电压保护参数
struct TempVoltageProtection {
  int protection_current_ma;     // 保护电流（mA）
  int high_temp_threshold;       // 高温阈值
  int low_temp_threshold;        // 低温阈值
  int protection_temp;           // 保护温度
  int recovery_temp;             // 恢复温度
  int protection_voltage;        // 保护电压
  int recovery_voltage;          // 恢复电压
  int protection_battery_level;  // 保护电量
  int limit_run_battery_level;   // 限制运行电量
  int recovery_battery_level;    // 恢复电量
  int board_protection_temp;     // 主板保护温度
  int board_recovery_temp;       // 主板恢复温度
};

// 机器人本地时间
struct RobotLocalTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  int weekday;  // 星期
};

// 环境信息
struct EnvironmentInfo {
  float sensor_temperature;   // 温湿度传感器温度
  float sensor_humidity;      // 温湿度传感器湿度
  float ambient_temperature;  // 环境温度
  int day_night_status;       // 白夜状态
};

// 机器人数据结构
struct RobotData {
  // 电流电压
  int main_motor_current;   // 主电机电流（100mA）
  int slave_motor_current;  // 从电机电流（100mA）
  int battery_voltage;      // 电池电压（100mV）
  int battery_current;      // 电池电流（100mA）

  // 电池信息
  int battery_status;       // 电池状态
  int battery_level;        // 电池电量
  int battery_temperature;  // 电池温度

  // 位置和运行信息
  std::string position_info;  // 位置信息
  int working_duration;       // 工作时长
  int total_run_count;        // 累计运行次数
  int current_lap_count;      // 当前运行圈数

  // 光伏信息
  int solar_voltage;  // 光伏板输出电压（100mV）
  int solar_current;  // 光伏板输出电流（100mA）

  // 时间戳
  struct {
    int hour;
    int minute;
    int second;
  } current_timestamp;  // 当前变化时间戳

  // 温度
  int board_temperature;  // 主板温度

  // 配置参数
  LoraParams lora_params;                    // LoRa参数
  std::string robot_number;                  // 机器人编号
  std::string software_version;              // 软件版本
  int parking_position;                      // 停机位
  bool daytime_scan_protect;                 // 白天防误扫开关
  std::vector<ScheduleTask> schedule_tasks;  // 定时任务
  bool enabled;                              // 启用/停用

  // 电机和保护参数
  MotorParams motor_params;
  TempVoltageProtection temp_voltage_protection;

  // 时间和环境
  RobotLocalTime local_time;
  EnvironmentInfo environment_info;

  // 数组数据
  std::vector<int> master_currents;  // 主机电流
  std::vector<int> slave_currents;   // 从机电流
  int position;                      // 位置
  int direction;                     // 方向

  // 设备标识
  std::string module_eui;     // 模组EUI
  int domestic_foreign_flag;  // 国内外版本标识
  std::string country_code;   // 国家代码
  std::string region_code;    // 地区代码
  std::string project_code;   // 项目代码
};

class Robot {
 public:
  explicit Robot(const std::string& robot_id);
  ~Robot();

  // 获取机器人ID
  std::string GetId() const { return robot_id_; }

  // 设置主题
  void SetTopics(const std::string& publish_topic,
                 const std::string& subscribe_topic);

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

 private:
  std::string robot_id_;
  std::string publish_topic_;
  std::string subscribe_topic_;
  std::atomic<int> sequence_;
  RobotData data_;  // 机器人完整数据

  // 读取上行数据模板
  static std::string LoadUplinkTemplate();
  static std::string uplink_template_;  // 静态模板缓存
};

#endif  // ROBOT_H_
