#include "mqtt_manager.h"

#include <glog/logging.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

MqttManager::MqttManager(const std::string& broker,
                         const std::string& client_id, int qos,
                         ConfigDb& config_db)
    : broker_(broker), client_id_(client_id), qos_(qos),
      config_db_(config_db) {
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

void MqttManager::PublishRaw(const std::string& topic, const std::string& payload) {
  try {
    auto msg = mqtt::make_message(topic, payload);
    msg->set_qos(qos_);
    client_->publish(msg)->wait();
    LOG(INFO) << "已向主题发布原始消息: " << topic << " -> " << payload;
  } catch (const mqtt::exception& exc) {
    LOG(ERROR) << "原始发布失败: " << exc.what();
  }
}

void MqttManager::PublishAll() {
  std::vector<std::shared_ptr<Robot>> copy;
  {
    std::lock_guard<std::mutex> lock(robots_mutex_);
    for (const auto& kv : robots_) copy.push_back(kv.second);
  }

  for (const auto& robot : copy) {
    Publish(robot->GetId());
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

bool MqttManager::Run(int keepalive, int duration_seconds, int publish_interval_seconds) {
  if (running_.load()) return false;
  running_.store(true);

  if (!Connect(keepalive)) {
    running_.store(false);
    return false;
  }

  // 初始加载机器人并注册
  RefreshRobots();

  // 启动后台刷新线程
  stop_refresh_.store(false);
  refresher_thread_ = std::thread([this]() {
    while (!stop_refresh_.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
      RefreshRobots();
    }
  });

  // 发布循环
  for (int i = 0; i < duration_seconds; i += publish_interval_seconds) {
    PublishAll();
    std::this_thread::sleep_for(std::chrono::seconds(publish_interval_seconds));
  }

  Stop();
  return true;
}

void MqttManager::Stop() {
  if (!running_.load()) return;
  stop_refresh_.store(true);
  if (refresher_thread_.joinable()) refresher_thread_.join();
  Disconnect();
  running_.store(false);
}

void MqttManager::connection_lost(const std::string& cause) {
  LOG(WARNING) << "Connection lost: " << cause;
}

void MqttManager::message_arrived(mqtt::const_message_ptr msg) {
  std::string topic = msg->get_topic();
  std::string payload = msg->to_string();

  LOG(INFO) << "收到消息 - 主题: " << topic;

  try {
    // 解析下行JSON数据
    json j = json::parse(payload);

    // 提取devEui和data字段
    if (!j.contains("devEui") || !j.contains("data")) {
      LOG(WARNING) << "消息缺少必需字段 devEui 或 data";
      return;
    }

    std::string dev_eui = j["devEui"].get<std::string>();
    std::string data = j["data"].get<std::string>();

    // 根据devEui查找对应的机器人
    auto robot_it = robots_.find(dev_eui);
    if (robot_it != robots_.end()) {
      LOG(INFO) << "将消息路由到机器人: " << dev_eui;
      robot_it->second->HandleMessage(data);
    } else {
      LOG(WARNING) << "未找到devEui对应的机器人: " << dev_eui;
    }
  } catch (const json::exception& e) {
    LOG(ERROR) << "JSON解析失败: " << e.what();
  }
}

void MqttManager::delivery_complete(mqtt::delivery_token_ptr token) {}
