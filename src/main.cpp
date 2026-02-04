#include <glog/logging.h>
#include <sys/stat.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "config_db.h"
#include "mqtt_manager.h"
#include "robot.h"

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
  // 创建日志目录
  mkdir("./logs", 0755);

  // 初始化glog
  google::InitGoogleLogging(argv[0]);

  // 配置日志输出到文件和终端
  FLAGS_alsologtostderr = true;            // 同时输出到文件和终端
  FLAGS_colorlogtostderr = true;           // 终端输出带颜色
  FLAGS_log_dir = "./logs";                // 日志文件目录
  FLAGS_max_log_size = 100;                // 单个日志文件最大100MB
  FLAGS_stop_logging_if_full_disk = true;  // 磁盘满时停止日志

  // 打印版本信息
  LOG(INFO) << "Robot MQTT Simulator v" << PROJECT_VERSION;

  // 初始化数据库
  ConfigDb config_db("config.db");
  if (!config_db.Init()) {
    LOG(ERROR) << "Failed to initialize database";
    return 1;
  }

  // 从数据库加载配置
  std::string broker =
      config_db.GetValue("broker", "tcp://test.mosquitto.org:1883");
  std::string client_id_prefix =
      config_db.GetValue("client_id_prefix", "sim_robot_cpp");
  int qos = config_db.GetIntValue("qos", 1);
  int keepalive = config_db.GetIntValue("keepalive", 60);
  int publish_interval = config_db.GetIntValue("publish_interval", 10);
  int duration = config_db.GetIntValue("default_duration", 30);

  // 获取启用的机器人列表
  auto enabled_robots = config_db.GetEnabledRobots();
  if (enabled_robots.empty()) {
    LOG(ERROR) << "没有启用的机器人";
    return 1;
  }

  const std::string client_id = client_id_prefix;

  LOG(INFO) << "=== 配置信息 ===";
  LOG(INFO) << "Broker: " << broker;
  LOG(INFO) << "Client ID: " << client_id;
  LOG(INFO) << "QoS: " << qos;
  LOG(INFO) << "Duration: " << duration << " seconds";
  LOG(INFO) << "启用的机器人 (" << enabled_robots.size() << "):";
  for (const auto& id : enabled_robots) LOG(INFO) << "  - " << id;
  LOG(INFO) << "==================";

  // 创建并运行 MQTT 管理器（内部会负责加载机器人、订阅与定期刷新）
  MqttManager mqtt_manager(broker, client_id, qos, config_db);
  if (!mqtt_manager.Run(keepalive)) {
    LOG(ERROR) << "MQTT 管理器运行失败";
    return 1;
  }

  // 保持程序运行，不退出
  LOG(INFO) << "程序运行中，按 Ctrl+C 退出...";
  while (true) {
    std::this_thread::sleep_for(1s);
  }

  return 0;
}
