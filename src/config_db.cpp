#include "config_db.h"

#include <glog/logging.h>
#include <fstream>
#include <string>

ConfigDb::ConfigDb(const std::string& path)
    : db_(nullptr), db_path_(path), initialized_(false) {
  initialized_ = Init();
  if (!initialized_) {
    LOG(ERROR) << "数据库初始化失败: " << db_path_;
  }
}

ConfigDb::~ConfigDb() {
  if (db_) {
    sqlite3_close(db_);
  }
}

bool ConfigDb::Init() {
  // 检查数据库文件是否存在
  bool db_exists = false;
  {
    std::ifstream file(db_path_);
    db_exists = file.good();
  }

  if (!db_exists) {
    LOG(INFO) << "数据库文件不存在，将创建新数据库: " << db_path_;
  } else {
    LOG(INFO) << "正在打开数据库: " << db_path_;
  }

  int rc = sqlite3_open(db_path_.c_str(), &db_);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Cannot open database: " << sqlite3_errmsg(db_);
    return false;
  }

  LOG(INFO) << "数据库已打开，正在创建表结构...";

  // 创建配置表
  const char* sql = R"(
    CREATE TABLE IF NOT EXISTS mqtt_config (
      key TEXT PRIMARY KEY,
      value TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS robots (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      robot_id TEXT UNIQUE NOT NULL,
      robot_name TEXT,
      serial_number INTEGER UNIQUE NOT NULL DEFAULT 0,
      enabled INTEGER DEFAULT 1,
      alarm_fa INTEGER DEFAULT 0,
      alarm_fb INTEGER DEFAULT 0,
      alarm_fc INTEGER DEFAULT 0,
      alarm_fd INTEGER DEFAULT 0
    );
  )";

  char* err_msg = nullptr;
  rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "SQL error: " << err_msg;
    sqlite3_free(err_msg);
    return false;
  }

  LOG(INFO) << "表结构创建成功";

  // 插入默认配置（如果不存在）
  InsertDefaultConfig();

  LOG(INFO) << "数据库初始化完成";
  return true;
}

void ConfigDb::InsertDefaultConfig() {
  sqlite3_stmt* stmt;

  // 检查mqtt_config表
  const char* check_config_sql = "SELECT COUNT(*) FROM mqtt_config";
  bool has_config = false;

  if (sqlite3_prepare_v2(db_, check_config_sql, -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      int count = sqlite3_column_int(stmt, 0);
      has_config = (count > 0);
    }
    sqlite3_finalize(stmt);
  }

  if (!has_config) {
    LOG(INFO) << "插入默认MQTT配置...";
    const char* config_sql = R"(
      INSERT OR IGNORE INTO mqtt_config (key, value) VALUES
      ('broker', 'tcp://lanq.top:10043'),
      ('client_id_prefix', 'sim_robot_cpp'),
      ('qos', '1'),
      ('keepalive', '60'),
      ('publish_interval', '10'),
      ('http_port', '8080'),
      ('publish_topic', 'application/902d7d6e-d3ac-44c0-a128-6d6743ba2b59/device/{robot_id}/event/up'),
      ('subscribe_topic', 'application/902d7d6e-d3ac-44c0-a128-6d6743ba2b59/device/{robot_id}/command/down')
    )";

    char* err_msg = nullptr;
    if (sqlite3_exec(db_, config_sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
      LOG(ERROR) << "插入默认配置失败: " << err_msg;
      sqlite3_free(err_msg);
    } else {
      LOG(INFO) << "默认MQTT配置插入成功";
    }
  } else {
    LOG(INFO) << "mqtt_config表已有配置，跳过配置插入";
  }

  // 检查robots表
  const char* check_robots_sql = "SELECT COUNT(*) FROM robots";
  bool has_robots = false;

  if (sqlite3_prepare_v2(db_, check_robots_sql, -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      int count = sqlite3_column_int(stmt, 0);
      has_robots = (count > 0);
    }
    sqlite3_finalize(stmt);
  }

  if (!has_robots) {
    LOG(INFO) << "插入默认机器人...";
    const char* robots_sql = R"(
      INSERT OR IGNORE INTO robots (robot_id, robot_name, serial_number, enabled) VALUES
      ('303930306350729d', 'Robot 1', 1, 1),
      ('303930306350729e', 'Robot 2', 2, 1),
      ('303930306350729f', 'Robot 3', 3, 0)
    )";

    char* err_msg = nullptr;
    if (sqlite3_exec(db_, robots_sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
      LOG(ERROR) << "插入默认机器人失败: " << err_msg;
      sqlite3_free(err_msg);
    } else {
      LOG(INFO) << "默认机器人插入成功";
    }
  } else {
    LOG(INFO) << "robots表已有机器人，跳过机器人插入";
  }
}

std::string ConfigDb::GetValue(const std::string& key,
                               const std::string& default_value) {
  std::string sql = "SELECT value FROM mqtt_config WHERE key = ?";
  sqlite3_stmt* stmt;
  std::string result = default_value;

  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      const char* value =
          reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      if (value) result = value;
    }
    sqlite3_finalize(stmt);
  }
  return result;
}

int ConfigDb::GetIntValue(const std::string& key, int default_value) {
  std::string value = GetValue(key);
  return value.empty() ? default_value : std::stoi(value);
}

std::vector<std::string> ConfigDb::GetEnabledRobots() {
  std::vector<std::string> robots;
  std::string sql = "SELECT robot_id FROM robots WHERE enabled = 1";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char* robot_id =
          reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      if (robot_id) {
        robots.push_back(robot_id);
      }
    }
    sqlite3_finalize(stmt);
  }
  return robots;
}

std::string ConfigDb::GetPublishTopic(const std::string& robot_id) {
  std::string topic_template = GetValue("publish_topic", "");
  return ReplacePlaceholder(topic_template, robot_id);
}

std::string ConfigDb::GetSubscribeTopic(const std::string& robot_id) {
  std::string topic_template = GetValue("subscribe_topic", "");
  return ReplacePlaceholder(topic_template, robot_id);
}

std::string ConfigDb::ReplacePlaceholder(const std::string& topic_template,
                                         const std::string& robot_id) {
  std::string topic = topic_template;
  const std::string placeholder = "{robot_id}";
  size_t pos = topic.find(placeholder);
  while (pos != std::string::npos) {
    topic.replace(pos, placeholder.length(), robot_id);
    pos = topic.find(placeholder, pos + robot_id.length());
  }
  return topic;
}

bool ConfigDb::AddRobot(const std::string& robot_id,
                        const std::string& robot_name, int serial_number, bool enabled) {
  if (!initialized_) return false;

  const char* sql =
      "INSERT OR REPLACE INTO robots (robot_id, robot_name, serial_number, enabled) VALUES "
      "(?, ?, ?, ?)";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, robot_id.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, robot_name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 3, serial_number);
  sqlite3_bind_int(stmt, 4, enabled ? 1 : 0);

  bool success = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);

  return success;
}

bool ConfigDb::RemoveRobot(const std::string& robot_id) {
  if (!initialized_) return false;

  const char* sql = "DELETE FROM robots WHERE robot_id = ?";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, robot_id.c_str(), -1, SQLITE_STATIC);

  bool success = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);

  return success;
}

bool ConfigDb::UpdateRobotStatus(const std::string& robot_id, bool enabled) {
  if (!initialized_) return false;

  const char* sql = "UPDATE robots SET enabled = ? WHERE robot_id = ?";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_int(stmt, 1, enabled ? 1 : 0);
  sqlite3_bind_text(stmt, 2, robot_id.c_str(), -1, SQLITE_STATIC);

  bool success = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);

  return success;
}

bool ConfigDb::IsSerialNumberExists(int serial_number) {
  if (!initialized_) return false;

  const char* sql = "SELECT COUNT(*) FROM robots WHERE serial_number = ?";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_int(stmt, 1, serial_number);

  bool exists = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int count = sqlite3_column_int(stmt, 0);
    exists = (count > 0);
  }

  sqlite3_finalize(stmt);
  return exists;
}

int ConfigDb::GetMaxSerialNumber() {
  if (!initialized_) return 0;

  const char* sql = "SELECT MAX(serial_number) FROM robots";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return 0;
  }

  int max_serial = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    max_serial = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return max_serial;
}

std::string ConfigDb::GetRobotIdBySerial(int serial_number) {
  if (!initialized_) return "";

  const char* sql = "SELECT robot_id FROM robots WHERE serial_number = ? AND enabled = 1";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return "";
  }

  sqlite3_bind_int(stmt, 1, serial_number);

  std::string robot_id;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (id) robot_id = id;
  }

  sqlite3_finalize(stmt);
  return robot_id;
}

std::vector<ConfigDb::RobotInfo> ConfigDb::GetAllRobots() {
  std::vector<RobotInfo> robots;
  if (!initialized_) return robots;

  const char* sql = "SELECT robot_id, robot_name, serial_number, enabled FROM robots ORDER BY serial_number ASC";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return robots;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    RobotInfo info;
    info.robot_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    info.robot_name = name ? name : "";
    info.serial_number = sqlite3_column_int(stmt, 2);
    info.enabled = sqlite3_column_int(stmt, 3) != 0;
    robots.push_back(info);
  }

  sqlite3_finalize(stmt);
  return robots;
}

bool ConfigDb::AddRobotsBatch(const std::vector<RobotInfo>& robots) {
  if (!initialized_ || robots.empty()) return false;

  // 开始事务
  char* err_msg = nullptr;
  if (sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &err_msg) != SQLITE_OK) {
    LOG(ERROR) << "开始事务失败: " << err_msg;
    sqlite3_free(err_msg);
    return false;
  }

  const char* sql = "INSERT OR REPLACE INTO robots (robot_id, robot_name, serial_number, enabled) VALUES (?, ?, ?, ?)";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
    return false;
  }

  for (const auto& robot : robots) {
    sqlite3_bind_text(stmt, 1, robot.robot_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, robot.robot_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, robot.serial_number);
    sqlite3_bind_int(stmt, 4, robot.enabled ? 1 : 0);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      LOG(ERROR) << "批量添加机器人失败: " << robot.robot_id;
      sqlite3_finalize(stmt);
      sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
      return false;
    }

    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);

  // 提交事务
  if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &err_msg) != SQLITE_OK) {
    LOG(ERROR) << "提交事务失败: " << err_msg;
    sqlite3_free(err_msg);
    sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
    return false;
  }

  return true;
}

bool ConfigDb::RemoveRobotsBatch(const std::vector<std::string>& robot_ids) {
  if (!initialized_ || robot_ids.empty()) return false;

  // 开始事务
  char* err_msg = nullptr;
  if (sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &err_msg) != SQLITE_OK) {
    LOG(ERROR) << "开始事务失败: " << err_msg;
    sqlite3_free(err_msg);
    return false;
  }

  const char* sql = "DELETE FROM robots WHERE robot_id = ?";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
    return false;
  }

  for (const auto& robot_id : robot_ids) {
    sqlite3_bind_text(stmt, 1, robot_id.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      LOG(ERROR) << "批量删除机器人失败: " << robot_id;
      sqlite3_finalize(stmt);
      sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
      return false;
    }

    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);

  // 提交事务
  if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &err_msg) != SQLITE_OK) {
    LOG(ERROR) << "提交事务失败: " << err_msg;
    sqlite3_free(err_msg);
    sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
    return false;
  }

  return true;
}

// 更新机器人告警
bool ConfigDb::UpdateRobotAlarms(const std::string& robot_id, const AlarmData& alarms) {
  const char* sql = "UPDATE robots SET alarm_fa = ?, alarm_fb = ?, alarm_fc = ?, alarm_fd = ? WHERE robot_id = ?";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG(ERROR) << "准备更新告警SQL失败: " << sqlite3_errmsg(db_);
    return false;
  }

  sqlite3_bind_int(stmt, 1, alarms.alarm_fa);
  sqlite3_bind_int(stmt, 2, alarms.alarm_fb);
  sqlite3_bind_int(stmt, 3, alarms.alarm_fc);
  sqlite3_bind_int(stmt, 4, alarms.alarm_fd);
  sqlite3_bind_text(stmt, 5, robot_id.c_str(), -1, SQLITE_STATIC);

  bool success = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);

  if (success) {
    LOG(INFO) << "更新机器人告警成功: " << robot_id;
  } else {
    LOG(ERROR) << "更新机器人告警失败: " << sqlite3_errmsg(db_);
  }

  return success;
}

// 获取机器人告警
ConfigDb::AlarmData ConfigDb::GetRobotAlarms(const std::string& robot_id) {
  AlarmData alarms = {0, 0, 0, 0};

  const char* sql = "SELECT alarm_fa, alarm_fb, alarm_fc, alarm_fd FROM robots WHERE robot_id = ?";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG(ERROR) << "准备查询告警SQL失败: " << sqlite3_errmsg(db_);
    return alarms;
  }

  sqlite3_bind_text(stmt, 1, robot_id.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    alarms.alarm_fa = sqlite3_column_int(stmt, 0);
    alarms.alarm_fb = sqlite3_column_int(stmt, 1);
    alarms.alarm_fc = sqlite3_column_int(stmt, 2);
    alarms.alarm_fd = sqlite3_column_int(stmt, 3);
  } else {
    LOG(WARNING) << "未找到机器人告警数据: " << robot_id;
  }

  sqlite3_finalize(stmt);
  return alarms;
}
