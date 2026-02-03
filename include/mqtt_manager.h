#ifndef MQTT_MANAGER_H_
#define MQTT_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "config_db.h"
#include "mqtt/async_client.h"
#include "robot.h"

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
};

#endif  // MQTT_MANAGER_H_
