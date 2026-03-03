#include <glog/logging.h>
#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "config_db.h"
#include "http_server.h"
#include "mqtt_manager.h"
#include "robot.h"
#include "version.h"

using namespace std::chrono_literals;

namespace {
constexpr size_t kMaxLogFilesPerSeverity = 10;
constexpr auto kLogCleanupInterval = std::chrono::minutes(5);

void CleanupOldLogFiles(const std::string& log_dir,
                        size_t max_files_per_severity) {
  namespace fs = std::filesystem;

  std::error_code ec;
  if (!fs::exists(log_dir, ec) || !fs::is_directory(log_dir, ec)) {
    return;
  }

  const std::vector<std::string> severities = {"INFO", "WARNING", "ERROR", "FATAL"};
  std::unordered_map<std::string, std::vector<fs::directory_entry>> files_by_severity;

  for (const auto& entry : fs::directory_iterator(log_dir, ec)) {
    if (ec || !entry.is_regular_file(ec)) {
      continue;
    }

    const std::string filename = entry.path().filename().string();
    for (const auto& severity : severities) {
      if (filename.find("." + severity + ".") != std::string::npos) {
        files_by_severity[severity].push_back(entry);
        break;
      }
    }
  }

  for (const auto& severity : severities) {
    auto it = files_by_severity.find(severity);
    if (it == files_by_severity.end() || it->second.size() <= max_files_per_severity) {
      continue;
    }

    auto& files = it->second;
    std::sort(files.begin(), files.end(), [](const fs::directory_entry& a,
                                             const fs::directory_entry& b) {
      std::error_code a_ec;
      std::error_code b_ec;
      const auto a_time = a.last_write_time(a_ec);
      const auto b_time = b.last_write_time(b_ec);
      if (a_ec || b_ec) {
        return a.path().filename().string() > b.path().filename().string();
      }
      return a_time > b_time;
    });

    for (size_t i = max_files_per_severity; i < files.size(); ++i) {
      std::error_code remove_ec;
      fs::remove(files[i].path(), remove_ec);
    }
  }
}

void StartPeriodicLogCleanup(const std::string& log_dir,
                             size_t max_files_per_severity,
                             std::chrono::minutes interval) {
  std::thread([log_dir, max_files_per_severity, interval]() {
    while (true) {
      CleanupOldLogFiles(log_dir, max_files_per_severity);
      std::this_thread::sleep_for(interval);
    }
  }).detach();
}
}  // namespace

  // 测试函数：动态添加和删除机器人
void TestAddRemoveRobot(std::shared_ptr<MqttManager> mqtt_manager,
                        std::shared_ptr<ConfigDb> config_db) {
  const std::string test_robot_id = "303930306350729g";

  // 等待30秒后新增机器人
  LOG(INFO) << "等待30秒后进行测试...";
  std::this_thread::sleep_for(std::chrono::seconds(30));

  LOG(INFO) << "=== 测试：新增机器人 " << test_robot_id << " ===";
  // 添加到数据库（serial_number使用99作为测试值）
  if (config_db->AddRobot(test_robot_id, "Test Robot", 99, true)) {
    LOG(INFO) << "数据库中已添加机器人";

    // 添加到MqttManager
    mqtt_manager->AddRobot(test_robot_id);
    LOG(INFO) << "MqttManager中已添加机器人";
  } else {
    LOG(ERROR) << "添加机器人到数据库失败";
  }

  // 再等待30秒后删除机器人
  std::this_thread::sleep_for(std::chrono::seconds(30));

  LOG(INFO) << "=== 测试：删除机器人 " << test_robot_id << " ===";
  // 从MqttManager删除
  mqtt_manager->RemoveRobot(test_robot_id);
  LOG(INFO) << "MqttManager中已删除机器人";

  // 从数据库删除
  if (config_db->RemoveRobot(test_robot_id)) {
    LOG(INFO) << "数据库中已删除机器人";
  } else {
    LOG(ERROR) << "从数据库删除机器人失败";
  }

  LOG(INFO) << "测试完成";
}

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

  // 按级别定时清理日志文件（每个级别最多保留10个）
  CleanupOldLogFiles(FLAGS_log_dir, kMaxLogFilesPerSeverity);
  StartPeriodicLogCleanup(FLAGS_log_dir, kMaxLogFilesPerSeverity,
                          kLogCleanupInterval);

  // 打印版本信息
  LOG(INFO) << "================================================";
  LOG(INFO) << "  Robot MQTT Simulator  v" << APP_VERSION_STR;
  LOG(INFO) << "================================================";

  // 初始化数据库
  auto config_db = std::make_shared<ConfigDb>("config.db");
  if (!config_db->Init()) {
    LOG(ERROR) << "Failed to initialize database";
    return 1;
  }

  // 从数据库加载配置
  std::string broker =
      config_db->GetValue("broker", "tcp://test.mosquitto.org:1883");
  std::string client_id_prefix =
      config_db->GetValue("client_id_prefix", "sim_robot_cpp");
  int qos = config_db->GetIntValue("qos", 1);
  int keepalive = config_db->GetIntValue("keepalive", 60);
  int publish_interval = config_db->GetIntValue("publish_interval", 10);
  int http_port = config_db->GetIntValue("http_port", 8080);

  // 获取启用的机器人列表
  auto enabled_robots = config_db->GetEnabledRobots();
  if (enabled_robots.empty()) {
    LOG(ERROR) << "没有启用的机器人";
    return 1;
  }

  const std::string client_id = client_id_prefix;

  LOG(INFO) << "=== 配置信息 ===";
  LOG(INFO) << "Broker: " << broker;
  LOG(INFO) << "Client ID: " << client_id;
  LOG(INFO) << "QoS: " << qos;
  LOG(INFO) << "HTTP Port: " << http_port;
  LOG(INFO) << "启用的机器人 (" << enabled_robots.size() << "):";
  for (const auto& id : enabled_robots) LOG(INFO) << "  - " << id;
  LOG(INFO) << "==================";

  // 优先启动HTTP服务器，使Web页面尽快可用
  auto mqtt_manager =
      std::make_shared<MqttManager>(broker, client_id, qos, config_db);
  auto http_server =
      std::make_shared<HttpServer>(config_db, mqtt_manager, http_port);
  http_server->Start();

  // 随后启动 MQTT 管理器（连接远端 broker 可能耗时较长）
  if (!mqtt_manager->Run(keepalive)) {
    LOG(ERROR) << "MQTT 管理器运行失败";
    return 1;
  }

  // 保持程序运行，不退出
  LOG(INFO) << "程序运行中，按 Ctrl+C 退出...";
  LOG(INFO) << "HTTP服务器地址: http://localhost:" << http_port;

  // 启动测试：动态添加和删除机器人
  std::thread test_thread([mqtt_manager, config_db]() {
    TestAddRemoveRobot(mqtt_manager, config_db);
  });
  test_thread.detach();

  // 主循环保持程序运行
  while (true) {
    std::this_thread::sleep_for(1s);
  }

  return 0;
}
