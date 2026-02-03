#ifndef CONFIG_DB_H_
#define CONFIG_DB_H_

#include <sqlite3.h>

#include <string>
#include <vector>

class ConfigDb {
 private:
  sqlite3* db_;
  std::string db_path_;

  std::string ReplacePlaceholder(const std::string& topic_template,
                                 const std::string& robot_id);

 public:
  explicit ConfigDb(const std::string& path = "config.db");
  ~ConfigDb();

  bool Init();
  void InsertDefaultConfig();
  std::string GetValue(const std::string& key,
                       const std::string& default_value = "");
  int GetIntValue(const std::string& key, int default_value = 0);
  std::vector<std::string> GetEnabledRobots();
  std::string GetPublishTopic(const std::string& robot_id);
  std::string GetSubscribeTopic(const std::string& robot_id);
};

#endif  // CONFIG_DB_H_
