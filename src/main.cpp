#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "config_db.h"
#include "mqtt/async_client.h"
#include "mqtt_callback.h"

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
  // 初始化数据库
  ConfigDb config_db("config.db");
  if (!config_db.Init()) {
    std::cerr << "Failed to initialize database" << std::endl;
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
    std::cerr << "没有启用的机器人" << std::endl;
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

  std::cout << "=== 配置信息 ===" << std::endl;
  std::cout << "Broker: " << broker << std::endl;
  std::cout << "Robot ID: " << robot_id << std::endl;
  std::cout << "Client ID: " << client_id << std::endl;
  std::cout << "QoS: " << qos << std::endl;
  std::cout << "Duration: " << duration << " seconds" << std::endl;
  std::cout << "\n启用的机器人 (" << enabled_robots.size() << "):" << std::endl;
  for (const auto& id : enabled_robots) {
    std::cout << "  - " << id << (id == robot_id ? " (当前使用)" : "")
              << std::endl;
  }
  std::cout << "\n发布主题: " << publish_topic << std::endl;
  std::cout << "订阅主题: " << subscribe_topic << std::endl;
  std::cout << "==================\n" << std::endl;

  mqtt::async_client client(broker, client_id);
  MqttCallback cb;
  client.set_callback(cb);

  mqtt::connect_options conn_opts;
  conn_opts.set_keep_alive_interval(keepalive);

  try {
    std::cout << "正在连接到 broker: " << broker << std::endl;
    client.connect(conn_opts)->wait();
    std::cout << "连接成功!" << std::endl;

    // 订阅主题
    std::cout << "\n正在订阅主题: " << subscribe_topic << std::endl;
    client.subscribe(subscribe_topic, qos)->wait();
    std::cout << "订阅完成! 等待消息...\n" << std::endl;

    // 发布消息循环
    std::cout << "开始发布消息 (运行 " << duration << " 秒)..." << std::endl;
    for (int i = 0; i < duration; ++i) {
      std::string payload = "{\"seq\": " + std::to_string(i) +
                            ", \"battery\": " + std::to_string(100 - i % 100) +
                            ", \"robot_id\": \"" + robot_id + "\"}";

      // 发送消息
      auto msg = mqtt::make_message(publish_topic, payload);
      msg->set_qos(qos);
      client.publish(msg);

      std::cout << "[" << (i + 1) << "/" << duration << "] 已发布: " << payload
                << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(publish_interval));
    }

    std::cout << "\n发布完成，正在断开连接..." << std::endl;
    client.disconnect()->wait();
    std::cout << "已断开连接" << std::endl;
  } catch (const mqtt::exception& exc) {
    std::cerr << "MQTT 错误: " << exc.what() << std::endl;
    return 1;
  }

  return 0;
}
