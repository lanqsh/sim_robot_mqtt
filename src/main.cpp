#include <chrono>
#include <string>
#include <thread>

#include <glog/logging.h>
#include "config_db.h"
#include "mqtt/async_client.h"
#include "mqtt_callback.h"

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
  // 初始化glog
  google::InitGoogleLogging(argv[0]);

  // 配置日志输出到文件和终端
  FLAGS_alsologtostderr = true;        // 同时输出到文件和终端
  FLAGS_colorlogtostderr = true;       // 终端输出带颜色
  FLAGS_log_dir = "./logs";            // 日志文件目录
  FLAGS_max_log_size = 100;            // 单个日志文件最大100MB
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
  int publish_interval = config_db.GetIntValue("publish_interval", 1);
  int duration = config_db.GetIntValue("default_duration", 30);

  // 获取启用的机器人列表
  auto enabled_robots = config_db.GetEnabledRobots();
  if (enabled_robots.empty()) {
    LOG(ERROR) << "没有启用的机器人";
    return 1;
  }

  // 使用第一个启用的机器人
  std::string robot_id = enabled_robots[0];

  // 获取该机器人的主题
  std::string publish_topic = config_db.GetPublishTopic(robot_id);
  std::string subscribe_topic = config_db.GetSubscribeTopic(robot_id);

  // 命令行参数可以覆盖配置
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg.find("tcp://") == 0 || arg.find("ssl://") == 0) {
      broker = arg;
    } else if (std::isdigit(arg[0])) {
      duration = std::stoi(arg);
    }
  }

  const std::string client_id = client_id_prefix + "_" + robot_id;

  LOG(INFO) << "=== 配置信息 ===";
  LOG(INFO) << "Broker: " << broker;
  LOG(INFO) << "Robot ID: " << robot_id;
  LOG(INFO) << "Client ID: " << client_id;
  LOG(INFO) << "QoS: " << qos;
  LOG(INFO) << "Duration: " << duration << " seconds";
  LOG(INFO) << "启用的机器人 (" << enabled_robots.size() << "):";
  for (const auto& id : enabled_robots) {
    LOG(INFO) << "  - " << id << (id == robot_id ? " (当前使用)" : "");
  }
  LOG(INFO) << "发布主题: " << publish_topic;
  LOG(INFO) << "订阅主题: " << subscribe_topic;
  LOG(INFO) << "==================";

  mqtt::async_client client(broker, client_id);
  MqttCallback cb;
  client.set_callback(cb);

  mqtt::connect_options conn_opts;
  conn_opts.set_keep_alive_interval(keepalive);

  try {
    LOG(INFO) << "正在连接到 broker: " << broker;
    client.connect(conn_opts)->wait();
    LOG(INFO) << "连接成功!";

    // 订阅主题
    LOG(INFO) << "正在订阅主题: " << subscribe_topic;
    client.subscribe(subscribe_topic, qos)->wait();
    LOG(INFO) << "订阅完成! 等待消息...";

    // 发布消息循环
    LOG(INFO) << "开始发布消息 (运行 " << duration << " 秒)...";
    for (int i = 0; i < duration; ++i) {
      std::string payload = "{\"seq\": " + std::to_string(i) +
                            ", \"battery\": " + std::to_string(100 - i % 100) +
                            ", \"robot_id\": \"" + robot_id + "\"}";

      // 发送消息
      auto msg = mqtt::make_message(publish_topic, payload);
      msg->set_qos(qos);
      client.publish(msg);

      LOG(INFO) << "[" << (i + 1) << "/" << duration << "] 已发布: " << payload;
      std::this_thread::sleep_for(std::chrono::seconds(publish_interval));
    }

    LOG(INFO) << "发布完成，正在断开连接...";
    client.disconnect()->wait();
    LOG(INFO) << "已断开连接";
  } catch (const mqtt::exception& exc) {
    LOG(ERROR) << "MQTT 错误: " << exc.what();
    return 1;
  }

  google::ShutdownGoogleLogging();
  return 0;
}
