#ifndef MQTT_MANAGER_H_
#define MQTT_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "config_db.h"
#include "mqtt/async_client.h"
#include "robot.h"
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>

class MqttManager : public virtual mqtt::callback {
 public:
  MqttManager(const std::string& broker, const std::string& client_id, int qos,
              ConfigDb& config_db);
  ~MqttManager();

  // 连接到broker
  bool Connect(int keepalive);

  // 断开连接
  void Disconnect();

  // 添加机器人（自动从配置获取主题）
  void AddRobot(std::shared_ptr<Robot> robot);

  // 发布消息
  void Publish(const std::string& robot_id);

    // 发布所有已注册机器人的上行消息
    void PublishAll();

    // 直接向指定主题发布原始负载（用于测试下行）
    void PublishRaw(const std::string& topic, const std::string& payload);

    // 从配置库加载当前启用的机器人并注册（非阻塞）
    void RefreshRobots();

    // 运行完整流程：连接、加载机器人、周期发布、刷新新机器人
    bool Run(int keepalive, int duration_seconds, int publish_interval_seconds);

    // 停止运行（停止后台刷新并断开）
    void Stop();

  // MQTT回调
  void connection_lost(const std::string& cause) override;
  void message_arrived(mqtt::const_message_ptr msg) override;
  void delivery_complete(mqtt::delivery_token_ptr token) override;

 private:
  std::string broker_;
  std::string client_id_;
  int qos_;
  ConfigDb& config_db_;
  std::unique_ptr<mqtt::async_client> client_;
  std::map<std::string, std::shared_ptr<Robot>> robots_;  // robot_id -> Robot
  std::map<std::string, std::string>
      topic_to_robot_;  // subscribe_topic -> robot_id
    std::mutex robots_mutex_;
    std::thread refresher_thread_;
    std::atomic<bool> stop_refresh_{false};
    std::atomic<bool> running_{false};
};

#endif  // MQTT_MANAGER_H_
