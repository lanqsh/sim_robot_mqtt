#ifndef MQTT_MANAGER_H_
#define MQTT_MANAGER_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "config_db.h"
#include "mqtt/async_client.h"
#include "robot.h"

// 待发送的消息结构
struct PendingMessage {
  std::string topic;
  std::string payload;
  int qos;
};

// 接收到的消息结构
struct ReceivedMessage {
  std::string topic;
  std::string payload;
};

struct MqttCommMessage {
  std::string timestamp;
  std::string robot_id;
  std::string direction;
  std::string category;
  std::string command;
  std::string topic;
  std::string data;
};

class MqttManager : public virtual mqtt::callback,
                    public std::enable_shared_from_this<MqttManager> {
 public:
  MqttManager(const std::string& broker, const std::string& client_id, int qos,
              std::shared_ptr<ConfigDb> config_db);
  ~MqttManager();

  // 连接到broker
  bool Connect(int keepalive);

  // 断开连接
  void Disconnect();

  // 添加机器人（自动从配置获取主题）
  void AddRobot(std::shared_ptr<Robot> robot);

  // 添加机器人（通过robot_id）
  void AddRobot(const std::string& robot_id);

  // 删除机器人
  void RemoveRobot(const std::string& robot_id);

  // 获取机器人（用于HTTP API）
  std::shared_ptr<Robot> GetRobot(const std::string& robot_id);

  // 发布消息
  void Publish(const std::string& robot_id);

  // 将消息加入发送队列（线程安全，供Robot调用）
  void EnqueueMessage(const std::string& topic, const std::string& payload,
                      int qos);

  // 从配置库加载当前启用的机器人并注册（非阻塞）
  void RefreshRobots();

  // 运行完整流程：连接、加载机器人、启动发送和接收线程
  bool Run(int keepalive);

  // 停止运行（停止后台线程并断开）
  void Stop();

  // 实时更新所有运行中机器人的三类上报间隔并重启其上报线程
  void UpdateAllRobotsReportIntervals(int robot_data_s, int motor_params_s, int lora_clean_s);

  // 获取当前管理的机器人数量（供 Robot 内部计算错峰偏移用）
  int GetRobotCount();

  // 获取 MQTT 连接配置
  std::string GetBroker() const { return broker_; }
  std::string GetUsername() const { return username_; }
  bool IsConnected() const { return client_ && client_->is_connected(); }

  // 更新 MQTT 服务配置并重新连接（保存到数据库）
  bool ReconfigureAndReconnect(const std::string& broker,
                               const std::string& username,
                               const std::string& password,
                               int keepalive = 60);

  // MQTT回调
  void connection_lost(const std::string& cause) override;
  void message_arrived(mqtt::const_message_ptr msg) override;
  void delivery_complete(mqtt::delivery_token_ptr token) override;

  // Recent MQTT communication records (max 100)
  std::vector<MqttCommMessage> GetRecentMqttMessages(const std::string& robot_id,
                                                     const std::string& category,
                                                     const std::string& command,
                                                     const std::string& direction,
                                                     size_t limit = 100);

  // 全局数据模拟配置（对所有机器人通用）
  SimConfig GetGlobalSimConfig() const;
  void SetGlobalSimConfig(const SimConfig& config);

 private:
  std::string broker_;
  std::string username_;
  std::string password_;
  std::string client_id_;
  int qos_;
  std::shared_ptr<ConfigDb> config_db_;
  std::unique_ptr<mqtt::async_client> client_;
  std::map<std::string, std::shared_ptr<Robot>> robots_;  // robot_id -> Robot
  std::map<std::string, std::string>
      topic_to_robot_;  // subscribe_topic -> robot_id
  mutable std::mutex robots_mutex_;
  std::thread sender_thread_;    // 消息发送线程
  std::thread receiver_thread_;  // 消息接收处理线程
  std::atomic<bool> stop_sender_{false};
  std::atomic<bool> stop_receiver_{false};
  std::atomic<bool> running_{false};

  // 发送消息队列相关
  std::queue<PendingMessage> message_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;

  // 接收消息队列相关
  std::queue<ReceivedMessage> received_queue_;
  std::mutex received_queue_mutex_;
  std::condition_variable received_queue_cv_;

  // 全局数据模拟配置
  SimConfig global_sim_config_;
  mutable std::mutex sim_config_mutex_;

  // 后台发送线程函数
  void SenderThreadFunc();

  // 后台接收处理线程函数
  void ReceiverThreadFunc();

  // MQTT communication record cache
  void RecordMqttMessage(const std::string& direction,
                         const std::string& topic,
                         const std::string& payload);
  std::string BuildTimestampString() const;
  std::string ResolveCategoryByIdentifier(uint8_t identifier) const;
  std::string ResolveCommandByIdentifier(uint8_t identifier) const;
  std::string ResolveRobotIdByTopic(const std::string& topic) const;

  std::map<std::string, std::deque<MqttCommMessage>> comm_messages_by_robot_;
  mutable std::mutex comm_messages_mutex_;
};

#endif  // MQTT_MANAGER_H_
