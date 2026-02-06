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
#include <chrono>

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

// FA告警位定义（32位）
enum class AlarmFA : uint32_t {
  kDeviceEnabled = 1 << 0,          // 设备启用/停用
  kQueueSwitching = 1 << 1,         // 启动队列切换中
  kAutoManual = 1 << 2,             // 自动/手动
  kStartFailed = 1 << 3,            // 启动失败
  kAutoRunning = 1 << 4,            // 自动运行中(指清扫中)
  kAutoCompleted = 1 << 5,          // 自动运行完成(指清扫完成)
  kAutoFailed = 1 << 6,             // 自动运行失败
  kForward = 1 << 7,                // 前进
  kBackward = 1 << 8,               // 后退
  kStopped = 1 << 9,                // 运行停止
  kNearTrigger = 1 << 10,           // 近端触发
  kFarTrigger = 1 << 11,            // 远端触发
  kEmergencyStop = 1 << 12,         // 急停状态
  kAutoResetting = 1 << 13,         // 自动复位中
  kAutoResetCompleted = 1 << 14,    // 自动复位完成
  kLowBatteryReturn = 1 << 15,      // 低电量返回停靠位成功
  kUpperLimitReturn = 1 << 16,      // 上限停机返回成功
  kDaytimeProtection = 1 << 17,     // 白天防误扫触发
  kDayNightSensor = 1 << 18,        // 白夜状态(光线传感器状态)
  kRunEnded = 1 << 19,              // 运行结束（含正常异常停止）
  kAuthorized = 1 << 20,            // 有无授权
  kUpperLimitOriginal = 1 << 21,    // 上限停机返回原位成功
  kUpperLimitPlatform = 1 << 22,    // 上限停机返回停机平台成功
  kBodyStuck = 1 << 23,             // 机身卡套
  kBodyStuckRecovered = 1 << 24,    // 机身卡套恢复
  kPlatformNotAllowed = 1 << 25,    // 平台不允许运行
  kAutoRequestTimeout = 1 << 26     // 自动运行请求回复超时
};

// FB告警位定义（32位）
enum class AlarmFB : uint32_t {
  kRemoteStart = 1 << 0,            // 遥控器启动
  kAppStart = 1 << 1,               // app控制启动
  kSerialStart = 1 << 2,            // 串口控制启动
  kScadaStart = 1 << 3,             // scada主动控制启动
  kScheduledStart = 1 << 4,         // 机器人定时启动
  kAbnormalReturnStart = 1 << 5,    // 机器人异常回退请求启动
  kPowerRestoreStart = 1 << 6,      // 断电或者重启之后等致复启动
  kCommLostRestart = 1 << 7,        // 长时间无法通信等致重启
  kNetworkRestart = 1 << 8,         // 机器人本人入网等致重启
  kUpgradeRestart = 1 << 9,         // 升级成功导致重启
  kCommandRestart = 1 << 10         // 命令重启
};

// FC告警位定义（32位）
enum class AlarmFC : uint32_t {
  kChargerCommFault = 1 << 0,       // 充放电控制器通信故障
  kBatteryCommFault = 1 << 1,       // 电池包通信故障
  kSpiStorageFault = 1 << 2,        // SPI存储模块通信故障
  kLowBatteryWarning = 1 << 3,      // 低手保护电量预警
  kTempHumidSensorFault = 1 << 4,   // 温湿度传感器通信故障
  kBatteryVoltageProtect = 1 << 5,  // 电池电压保护
  kBatteryTempProtect = 1 << 6,     // 电池温度保护（高温）
  kBatteryCurrentProtect = 1 << 7,  // 电池电流保护
  kLowBatteryProtect = 1 << 8,      // 低电量保护
  kMainMotorUpperLimit = 1 << 9,    // 主电机上限故障（上限停机）
  kSlaveMotorUpperLimit = 1 << 10,  // 从电机上限故障（上限停机）
  kNoSignal = 1 << 11,              // 远近端无信号
  kAutoRunTimeout = 1 << 12,        // 自动运行超时
  kLoraCommFault = 1 << 13,         // Lora通信故障(离线)
  kWindProtect = 1 << 14,           // 大风保护
  kHumidityProtect = 1 << 15,       // 湿度保护
  kBatteryUnderVoltage = 1 << 16,   // 电池放电欠压
  kBatteryDischargeTempFault = 1 << 17,  // 电池放电温度故障
  kBatteryOverCurrent = 1 << 18,    // 电池放电过流
  kBatteryShortCircuit = 1 << 19,   // 电池放电短路
  kBatteryChargeOverVoltage = 1 << 20,   // 电池充电过压
  kBatteryChargeOverTemp = 1 << 21, // 电池充电过温
  kBatteryLowOrDisconnect = 1 << 22,     // 电池超低压或者断线
  kBatteryLifeExpired = 1 << 23,    // 电池寿命到期
  kAngleSensorFault = 1 << 24,      // 角度传感器故障
  kSecondRunTimeout = 1 << 25,      // 二次运行超时
  kMainEndProtect = 1 << 26,        // 主末保护
  kAmbientTempFault = 1 << 27,      // 环境温度故障
  kBoardTempFault = 1 << 28,        // 主板温度故障
  kMainMotorCurrentSurge = 1 << 29, // 主电机瞬时电流过大故障
  kSlaveMotorCurrentSurge = 1 << 30 // 从电机瞬时电流过大故障
};

// FD告警位定义（32位）
enum class AlarmFD : uint32_t {
  kMainMotorCurrentWarning = 1 << 0,    // 主电机上限电流预警
  kSlaveMotorCurrentWarning = 1 << 1,   // 从电机上限电流预警
  kBatteryHighTempWarning = 1 << 2,     // 电池高温预警
  kBatteryLowTempWarning = 1 << 3,      // 电池低温预警
  kPowerLossWarning = 1 << 4            // 设备掉电预警
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
  explicit Robot(const std::string& robot_id, uint16_t robot_number);
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

  // 控制类指令响应（使用机器人数据格式，但标识符不同）
  void SendControlResponse(uint8_t control_identifier);

  // 重启响应（仅包含标识符，无机器人数据）
  void SendRestartResponse(uint8_t control_identifier);

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

  // 机器人创建时间（用于计算工作时长）
  std::chrono::system_clock::time_point creation_time_;

  // 更新时间相关字段（本地时间、当前时间戳、工作时长）
  void UpdateTimeFields();

  // 构建机器人数据域（标识符 + 46字节机器人状态数据）
  std::vector<uint8_t> BuildRobotDataField(uint8_t identifier);

  // 上报线程函数
  void ReportThreadFunc();

  // 读取上行数据模板
  static std::string LoadUplinkTemplate();
  static std::string uplink_template_;  // 静态模板缓存
};

#endif  // ROBOT_H_
