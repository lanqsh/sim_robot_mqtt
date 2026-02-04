#ifndef MQTT_MANAGER_H_
#define MQTT_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <queue>
#include <condition_variable>

#include "config_db.h"
#include "mqtt/async_client.h"
#include "robot.h"
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>

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

    // 将消息加入发送队列（线程安全，供Robot调用）
    void EnqueueMessage(const std::string& topic, const std::string& payload, int qos);

    // 直接向指定主题发布原始负载（用于测试下行）
    void PublishRaw(const std::string& topic, const std::string& payload);

    // 从配置库加载当前启用的机器人并注册（非阻塞）
    void RefreshRobots();

    // 运行完整流程：连接、加载机器人、启动发送和接收线程
    bool Run(int keepalive);

    // 停止运行（停止后台线程并断开）
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
    std::thread sender_thread_;  // 消息发送线程
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

    // 后台发送线程函数
    void SenderThreadFunc();

    // 后台接收处理线程函数
    void ReceiverThreadFunc();
};

#endif  // MQTT_MANAGER_H_
