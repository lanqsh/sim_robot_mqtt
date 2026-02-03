#ifndef ROBOT_H_
#define ROBOT_H_

#include <string>
#include <memory>
#include <atomic>

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

  // 生成要发布的消息负载
  std::string GeneratePayload();

  // 处理接收到的订阅消息
  void HandleMessage(const std::string& topic, const std::string& payload);

 private:
  std::string robot_id_;
  std::string publish_topic_;
  std::string subscribe_topic_;
  std::atomic<int> sequence_;
  int battery_level_;
};

#endif  // ROBOT_H_
