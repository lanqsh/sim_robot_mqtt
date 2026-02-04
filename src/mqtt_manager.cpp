#include "mqtt_manager.h"

#include <glog/logging.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

MqttManager::MqttManager(const std::string& broker,
                         const std::string& client_id, int qos,
                         ConfigDb& config_db)
    : broker_(broker), client_id_(client_id), qos_(qos), config_db_(config_db) {
  client_ = std::make_unique<mqtt::async_client>(broker_, client_id_);
  client_->set_callback(*this);
}

MqttManager::~MqttManager() {
  if (client_ && client_->is_connected()) {
    Disconnect();
  }
}

bool MqttManager::Connect(int keepalive) {
  try {
    mqtt::connect_options conn_opts;
    conn_opts.set_keep_alive_interval(keepalive);

    LOG(INFO) << "正在连接到 broker: " << broker_;
    client_->connect(conn_opts)->wait();
    LOG(INFO) << "连接成功!";
    return true;
  } catch (const mqtt::exception& exc) {
    LOG(ERROR) << "连接失败: " << exc.what();
    return false;
  }
}

void MqttManager::Disconnect() {
  try {
    LOG(INFO) << "正在断开连接...";
    client_->disconnect()->wait();
    LOG(INFO) << "已断开连接";
  } catch (const mqtt::exception& exc) {
    LOG(ERROR) << "断开连接失败: " << exc.what();
  }
}

void MqttManager::AddRobot(std::shared_ptr<Robot> robot) {
  std::string robot_id = robot->GetId();

  // 从配置获取主题模板并拼接
  std::string publish_topic = config_db_.GetPublishTopic(robot_id);
  std::string subscribe_topic = config_db_.GetSubscribeTopic(robot_id);

  // 设置机器人的主题
  robot->SetTopics(publish_topic, subscribe_topic);

  // 设置上报间隔
  int report_interval = config_db_.GetIntValue("publish_interval", 10);
  robot->SetReportInterval(report_interval);

  // 设置MQTT管理器（启动上报线程）
  robot->SetMqttManager(shared_from_this());

  {
    std::lock_guard<std::mutex> lock(robots_mutex_);
    if (robots_.find(robot_id) != robots_.end()) {
      LOG(INFO) << "机器人已存在: " << robot_id;
      return;
    }
    robots_[robot_id] = robot;
    topic_to_robot_[subscribe_topic] = robot_id;
  }

  LOG(INFO) << "添加机器人: " << robot_id;
  LOG(INFO) << "  发布主题: " << publish_topic;
  LOG(INFO) << "  订阅主题: " << subscribe_topic;

  // 订阅该机器人的主题
  try {
    LOG(INFO) << "正在订阅主题: " << subscribe_topic;
    client_->subscribe(subscribe_topic, qos_)->wait();
    LOG(INFO) << "订阅完成!";
  } catch (const mqtt::exception& exc) {
    LOG(ERROR) << "订阅失败: " << exc.what();
  }
}

void MqttManager::AddRobot(const std::string& robot_id) {
  std::string publish_topic = config_db_.GetPublishTopic(robot_id);
  std::string subscribe_topic = config_db_.GetSubscribeTopic(robot_id);

  auto robot = std::make_shared<Robot>(robot_id);
  robot->SetTopics(publish_topic, subscribe_topic);

  // 设置上报间隔
  int report_interval = config_db_.GetIntValue("publish_interval", 10);
  robot->SetReportInterval(report_interval);

  // 设置MQTT管理器（启动上报线程）
  robot->SetMqttManager(shared_from_this());

  {
    std::lock_guard<std::mutex> lock(robots_mutex_);
    if (robots_.find(robot_id) != robots_.end()) {
      LOG(INFO) << "机器人已存在: " << robot_id;
      return;
    }
    robots_[robot_id] = robot;
    topic_to_robot_[subscribe_topic] = robot_id;
  }

  LOG(INFO) << "添加机器人: " << robot_id;
  LOG(INFO) << "  发布主题: " << publish_topic;
  LOG(INFO) << "  订阅主题: " << subscribe_topic;

  // 订阅该机器人的主题
  try {
    LOG(INFO) << "正在订阅主题: " << subscribe_topic;
    client_->subscribe(subscribe_topic, qos_)->wait();
    LOG(INFO) << "订阅完成!";
  } catch (const mqtt::exception& exc) {
    LOG(ERROR) << "订阅失败: " << exc.what();
  }
}

void MqttManager::RemoveRobot(const std::string& robot_id) {
  std::shared_ptr<Robot> robot;
  std::string subscribe_topic;

  {
    std::lock_guard<std::mutex> lock(robots_mutex_);
    auto it = robots_.find(robot_id);
    if (it == robots_.end()) {
      LOG(WARNING) << "机器人不存在: " << robot_id;
      return;
    }

    robot = it->second;
    subscribe_topic = robot->GetSubscribeTopic();

    // 从映射中删除
    robots_.erase(it);
    topic_to_robot_.erase(subscribe_topic);
  }

  LOG(INFO) << "删除机器人: " << robot_id;
  LOG(INFO) << "  订阅主题: " << subscribe_topic;

  // 取消订阅该机器人的主题
  try {
    LOG(INFO) << "正在取消订阅主题: " << subscribe_topic;
    client_->unsubscribe(subscribe_topic)->wait();
    LOG(INFO) << "取消订阅完成!";
  } catch (const mqtt::exception& exc) {
    LOG(ERROR) << "取消订阅失败: " << exc.what();
  }
}

std::shared_ptr<Robot> MqttManager::GetRobot(const std::string& robot_id) {
  std::lock_guard<std::mutex> lock(robots_mutex_);
  auto it = robots_.find(robot_id);
  if (it != robots_.end()) {
    return it->second;
  }
  return nullptr;
}

void MqttManager::Publish(const std::string& robot_id) {
  std::shared_ptr<Robot> robot;
  {
    std::lock_guard<std::mutex> lock(robots_mutex_);
    auto it = robots_.find(robot_id);
    if (it == robots_.end()) {
      LOG(WARNING) << "未找到机器人: " << robot_id;
      return;
    }
    robot = it->second;
  }
  // TODO: 生成实际的通信数据
  std::string data = "aIIACwAB8ugW";  // 示例数据
  std::string payload = robot->GenerateUplinkPayload(data);
  std::string publish_topic = robot->GetPublishTopic();

  try {
    auto msg = mqtt::make_message(publish_topic, payload);
    msg->set_qos(qos_);
    client_->publish(msg);
    LOG(INFO) << "[" << robot_id << "] 已发布: " << payload;
  } catch (const mqtt::exception& exc) {
    LOG(ERROR) << "发布失败: " << exc.what();
  }
}

void MqttManager::RefreshRobots() {
  auto enabled = config_db_.GetEnabledRobots();
  std::vector<std::string> to_add;
  {
    std::lock_guard<std::mutex> lock(robots_mutex_);
    for (const auto& id : enabled) {
      if (robots_.find(id) == robots_.end()) to_add.push_back(id);
    }
  }

  for (const auto& id : to_add) {
    LOG(INFO) << "检测到新机器人, 添加: " << id;
    auto robot = std::make_shared<Robot>(id);
    AddRobot(robot);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

bool MqttManager::Run(int keepalive) {
  if (running_.load()) return false;
  running_.store(true);

  if (!Connect(keepalive)) {
    running_.store(false);
    return false;
  }

  // 初始加载机器人并注册
  RefreshRobots();

  // 启动后台发送线程
  stop_sender_.store(false);
  sender_thread_ = std::thread(&MqttManager::SenderThreadFunc, this);

  // 启动后台接收处理线程
  stop_receiver_.store(false);
  receiver_thread_ = std::thread(&MqttManager::ReceiverThreadFunc, this);

  return true;
}

void MqttManager::Stop() {
  if (!running_.load()) return;
  running_.store(false);  // 停止主循环

  // 停止发送线程
  stop_sender_.store(true);
  queue_cv_.notify_all();  // 唤醒发送线程
  if (sender_thread_.joinable()) sender_thread_.join();

  // 停止接收线程
  stop_receiver_.store(true);
  received_queue_cv_.notify_all();  // 唤醒接收线程
  if (receiver_thread_.joinable()) receiver_thread_.join();

  Disconnect();
}

void MqttManager::EnqueueMessage(const std::string& topic,
                                 const std::string& payload, int qos) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    message_queue_.push({topic, payload, qos});
  }
  queue_cv_.notify_one();  // 通知发送线程
}

void MqttManager::SenderThreadFunc() {
  LOG(INFO) << "消息发送线程已启动";

  while (!stop_sender_.load()) {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    // 等待队列中有消息或收到停止信号
    queue_cv_.wait(lock, [this] {
      return !message_queue_.empty() || stop_sender_.load();
    });

    // 处理队列中的所有消息
    while (!message_queue_.empty() && !stop_sender_.load()) {
      PendingMessage msg = message_queue_.front();
      message_queue_.pop();
      lock.unlock();  // 释放锁以便其他线程可以入队

      // 发送消息
      try {
        auto mqtt_msg = mqtt::make_message(msg.topic, msg.payload);
        mqtt_msg->set_qos(msg.qos);
        client_->publish(mqtt_msg);
        LOG(INFO) << "已从队列发送消息到主题: " << msg.topic;
      } catch (const mqtt::exception& exc) {
        LOG(ERROR) << "发送队列消息失败: " << exc.what();
      }

      lock.lock();  // 重新获取锁以检查队列
    }
  }

  LOG(INFO) << "消息发送线程已停止";
}

void MqttManager::ReceiverThreadFunc() {
  LOG(INFO) << "消息接收处理线程已启动";

  while (!stop_receiver_.load()) {
    std::unique_lock<std::mutex> lock(received_queue_mutex_);

    // 等待队列中有消息或收到停止信号
    received_queue_cv_.wait(lock, [this] {
      return !received_queue_.empty() || stop_receiver_.load();
    });

    // 处理队列中的所有消息
    while (!received_queue_.empty() && !stop_receiver_.load()) {
      ReceivedMessage msg = received_queue_.front();
      received_queue_.pop();
      lock.unlock();  // 释放锁以便其他线程可以入队

      // 处理接收到的消息
      try {
        // 解析下行JSON数据
        json j = json::parse(msg.payload);

        // 提取devEui和data字段
        if (!j.contains("devEui") || !j.contains("data")) {
          LOG(WARNING) << "消息缺少必需字段 devEui 或 data";
          lock.lock();
          continue;
        }

        std::string dev_eui = j["devEui"].get<std::string>();
        std::string data = j["data"].get<std::string>();

        // 检查主题中是否包含devEui
        if (msg.topic.find(dev_eui) == std::string::npos) {
          LOG(WARNING) << "主题中不包含devEui: " << dev_eui << ", 主题: " << msg.topic;
          lock.lock();
          continue;
        }

        // 根据devEui查找对应的机器人
        std::shared_ptr<Robot> robot;
        {
          std::lock_guard<std::mutex> robots_lock(robots_mutex_);
          auto robot_it = robots_.find(dev_eui);
          if (robot_it != robots_.end()) {
            robot = robot_it->second;
          }
        }

        if (robot) {
          LOG(INFO) << "将消息路由到机器人: " << dev_eui;
          robot->HandleMessage(data);
        } else {
          LOG(WARNING) << "未找到devEui对应的机器人: " << dev_eui;
        }
      } catch (const json::exception& e) {
        LOG(ERROR) << "JSON解析失败: " << e.what();
      }

      lock.lock();  // 重新获取锁以检查队列
    }
  }

  LOG(INFO) << "消息接收处理线程已停止";
}

void MqttManager::connection_lost(const std::string& cause) {
  LOG(WARNING) << "Connection lost: " << cause;
}

void MqttManager::message_arrived(mqtt::const_message_ptr msg) {
  std::string topic = msg->get_topic();
  std::string payload = msg->to_string();

  LOG(INFO) << "收到消息 - 主题: " << topic;

  // 将接收到的消息放入接收队列，由接收线程处理
  {
    std::lock_guard<std::mutex> lock(received_queue_mutex_);
    received_queue_.push({topic, payload});
  }
  received_queue_cv_.notify_one();  // 通知接收处理线程
}

void MqttManager::delivery_complete(mqtt::delivery_token_ptr token) {}
