#include "config_db.h"

#include <glog/logging.h>

ConfigDb::ConfigDb(const std::string& path) : db_(nullptr), db_path_(path) {}

ConfigDb::~ConfigDb() {
  if (db_) {
    sqlite3_close(db_);
  }
}

bool ConfigDb::Init() {
  int rc = sqlite3_open(db_path_.c_str(), &db_);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Cannot open database: " << sqlite3_errmsg(db_);
    return false;
  }

  // 创建配置表
  const char* sql = R"(
    CREATE TABLE IF NOT EXISTS mqtt_config (
      key TEXT PRIMARY KEY,
      value TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS robots (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      robot_id TEXT UNIQUE NOT NULL,
      enabled INTEGER DEFAULT 1
    );
  )";

  char* err_msg = nullptr;
  rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "SQL error: " << err_msg;
    sqlite3_free(err_msg);
    return false;
  }

  // 插入默认配置（如果不存在）
  InsertDefaultConfig();
  return true;
}

void ConfigDb::InsertDefaultConfig() {
  const char* check_sql = "SELECT COUNT(*) FROM mqtt_config";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, check_sql, -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      int count = sqlite3_column_int(stmt, 0);
      sqlite3_finalize(stmt);

      if (count > 0) return;  // 已有配置
    }
  }

  // 插入默认配置
  const char* insert_sql = R"(
    INSERT OR IGNORE INTO mqtt_config (key, value) VALUES
    ('broker', 'tcp://lanq.top:10043'),
    ('client_id_prefix', 'sim_robot_cpp'),
    ('qos', '1'),
    ('keepalive', '60'),
    ('publish_interval', '1'),
    ('default_duration', '30'),
    ('publish_topic', 'application/902d7d6e-d3ac-44c0-a128-6d6743ba2b59/device/{robot_id}/command/up'),
    ('subscribe_topic', 'application/902d7d6e-d3ac-44c0-a128-6d6743ba2b59/device/{robot_id}/command/down');

    INSERT OR IGNORE INTO robots (robot_id, enabled) VALUES
    ('303930306350729d', 1),
    ('303930306350729e', 0),
    ('303930306350729f', 0);
  )";

  char* err_msg = nullptr;
  sqlite3_exec(db_, insert_sql, nullptr, nullptr, &err_msg);
  if (err_msg) {
    sqlite3_free(err_msg);
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
