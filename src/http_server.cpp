#include "http_server.h"

#include <glog/logging.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <set>

#include "config_db.h"
#include "mqtt_manager.h"
#include "version.h"

// 使用cpp-httplib库
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

using json = nlohmann::json;

// 生成16位十六进制ID
static std::string GenerateUUID() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  std::stringstream ss;
  ss << std::hex;

  // 生成16位十六进制字符串
  for (int i = 0; i < 16; i++) {
    ss << dis(gen);
  }

  return ss.str();
}

HttpServer::HttpServer(std::shared_ptr<ConfigDb> config_db,
                       std::shared_ptr<MqttManager> mqtt_manager, int port)
    : config_db_(config_db),
      mqtt_manager_(mqtt_manager),
      port_(port),
      running_(false) {}

HttpServer::~HttpServer() { Stop(); }

void HttpServer::Start() {
  if (running_) {
    LOG(WARNING) << "HTTP服务器已经在运行";
    return;
  }

  running_ = true;
  server_thread_ = std::thread(&HttpServer::ServerThreadFunc, this);
  LOG(INFO) << "HTTP服务器启动在端口: " << port_;
}

void HttpServer::Stop() {
  if (!running_) return;

  running_ = false;
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
  LOG(INFO) << "HTTP服务器已停止";
}

void HttpServer::ServerThreadFunc() {
  httplib::Server svr;

  // CORS支持
  svr.set_default_headers({{"Access-Control-Allow-Origin", "*"},
                           {"Access-Control-Allow-Methods", "GET, POST, DELETE, PATCH, OPTIONS"},
                           {"Access-Control-Allow-Headers", "Content-Type"}});

  // OPTIONS请求处理（预检请求）
  svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, PATCH, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.status = 204;
  });

  // 主页 - 返回HTML页面
  svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
    std::ifstream file("web/html/index.html");
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      res.set_content(buffer.str(), "text/html; charset=utf-8");
    } else {
      res.status = 404;
      res.set_content("web/html/index.html not found", "text/plain");
    }
  });

  // CSS样式文件
  svr.Get(R"(/css/(.+\.css))", [](const httplib::Request& req, httplib::Response& res) {
    std::string filename = req.matches[1];
    std::string filepath = "web/css/" + filename;
    std::ifstream file(filepath);
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      res.set_content(buffer.str(), "text/css; charset=utf-8");
    } else {
      res.status = 404;
      res.set_content("style.css not found", "text/plain");
    }
  });

  // JavaScript模块文件（支持 /js/*.js 路径）
  svr.Get(R"(/js/(.+\.js))", [](const httplib::Request& req, httplib::Response& res) {
    std::string filename = req.matches[1];
    std::string filepath = "web/js/" + filename;

    std::ifstream file(filepath);
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      res.set_content(buffer.str(), "application/javascript; charset=utf-8");
    } else {
      res.status = 404;
      res.set_content("File not found: " + filepath, "text/plain");
    }
  });

  // API: 获取所有机器人列表
  svr.Get("/api/v1/robots/get", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      auto all_robots = config_db_->GetAllRobots();

      // 查询过滤参数（三选一）
      std::string filter_name     = req.has_param("robot_name") ? req.get_param_value("robot_name") : "";
      std::string filter_robot_id = req.has_param("robot_id")   ? req.get_param_value("robot_id")   : "";
      std::string filter_enabled  = req.has_param("enabled")     ? req.get_param_value("enabled")     : "";

      // 如果有查询条件，过滤列表
      if (!filter_name.empty() || !filter_robot_id.empty() || !filter_enabled.empty()) {
        // 小写转换辅助函数
        auto to_lower = [](std::string s) {
          std::transform(s.begin(), s.end(), s.begin(), ::tolower);
          return s;
        };
        std::string lname = to_lower(filter_name);
        std::string lid   = to_lower(filter_robot_id);

        all_robots.erase(std::remove_if(all_robots.begin(), all_robots.end(),
          [&](const ConfigDb::RobotInfo& r) {
            if (!lname.empty()) {
              return to_lower(r.robot_name).find(lname) == std::string::npos;
            }
            if (!lid.empty()) {
              return to_lower(r.robot_id).find(lid) == std::string::npos;
            }
            if (!filter_enabled.empty()) {
              bool want_enabled = (filter_enabled == "true" || filter_enabled == "1");
              return r.enabled != want_enabled;
            }
            return false;
          }), all_robots.end());
      }

      // 获取分页参数
      int page = 1;
      int pageSize = 20;

      if (req.has_param("page")) {
        page = std::stoi(req.get_param_value("page"));
        if (page < 1) page = 1;
      }

      if (req.has_param("pageSize")) {
        pageSize = std::stoi(req.get_param_value("pageSize"));
        if (pageSize < 1) pageSize = 20;
        if (pageSize > 1000) pageSize = 1000; // 限制最大值
      }

      // 计算统计信息
      int total = all_robots.size();
      int enabled_count = 0;
      int disabled_count = 0;
      int fault_count = 0;
      int normal_count = 0;

      for (const auto& robot : all_robots) {
        if (robot.enabled) {
          enabled_count++;
        } else {
          disabled_count++;
        }
        auto live = mqtt_manager_->GetRobot(robot.robot_id);
        if (live) {
          const auto& rd = live->GetData();
          if (rd.alarm_fa != 0 || rd.alarm_fb != 0 || rd.alarm_fc != 0 || rd.alarm_fd != 0) {
            fault_count++;
          } else {
            normal_count++;
          }
        } else {
          normal_count++;
        }
      }

      // 计算分页
      int totalPages = (total + pageSize - 1) / pageSize;
      int startIndex = (page - 1) * pageSize;
      int endIndex = std::min(startIndex + pageSize, total);

      // 获取当前页数据
      json data = json::array();
      for (int i = startIndex; i < endIndex; i++) {
        const auto& robot = all_robots[i];
        json robot_json;
        robot_json["robot_id"] = robot.robot_id;
        robot_json["robot_name"] = robot.robot_name;
        robot_json["serial_number"] = robot.serial_number;
        robot_json["enabled"] = robot.enabled;
        {
          auto live = mqtt_manager_->GetRobot(robot.robot_id);
          if (live) {
            const auto& rd = live->GetData();
            robot_json["software_version"] = rd.software_version;
            robot_json["fault_status"] = (rd.alarm_fa != 0 || rd.alarm_fb != 0 || rd.alarm_fc != 0 || rd.alarm_fd != 0);
          } else {
            robot_json["software_version"] = "";
            robot_json["fault_status"] = false;
          }
        }
        data.push_back(robot_json);
      }

      // 构建响应
      json response;
      response["data"] = data;
      response["pagination"] = {
        {"page", page},
        {"pageSize", pageSize},
        {"total", total},
        {"totalPages", totalPages}
      };
      response["statistics"] = {
        {"total", total},
        {"enabled", enabled_count},
        {"disabled", disabled_count},
        {"normal", normal_count},
        {"fault", fault_count}
      };

      res.set_content(response.dump(), "application/json");

      LOG(INFO) << "API: 获取机器人列表, 页: " << page << "/" << totalPages << ", 总数: " << total;
    } catch (const std::exception& e) {
      LOG(ERROR) << "获取机器人列表失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // API: 添加机器人
  svr.Post("/api/v1/robots/add", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      json body = json::parse(req.body);
      std::string robot_name = body.value("robot_name", "");
      int serial_number = body.value("serial_number", 0);
      bool enabled = body.value("enabled", true);

      // 检查三选一：robot_name / robot_id / serial_number 至少填一个
      bool has_robot_id  = body.contains("robot_id") && !body["robot_id"].get<std::string>().empty();
      bool has_robot_name = !robot_name.empty();
      bool has_serial     = serial_number > 0;
      if (!has_robot_id && !has_robot_name && !has_serial) {
        json error;
        error["success"] = false;
        error["error"] = "机器人名称、机器人ID、序号三选一，至少填写一项";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      // 如果未提供序号，自动生成
      if (serial_number <= 0) {
        serial_number = config_db_->GetMaxSerialNumber() + 1;
        LOG(INFO) << "自动生成序号: " << serial_number;
      }

      // 检查序号是否已存在
      if (config_db_->IsSerialNumberExists(serial_number)) {
        json error;
        error["success"] = false;
        error["error"] = "序号 " + std::to_string(serial_number) + " 已存在，请使用其他序号";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      // 处理 robot_id：有则校验唯一性，无则自动生成
      std::string robot_id;
      if (body.contains("robot_id") && !body["robot_id"].get<std::string>().empty()) {
        robot_id = body["robot_id"].get<std::string>();
        // 检查 robot_id 是否已存在
        auto all_robots = config_db_->GetAllRobots();
        for (const auto& r : all_robots) {
          if (r.robot_id == robot_id) {
            json error;
            error["success"] = false;
            error["error"] = "robot_id \"" + robot_id + "\" 已存在，请使用其他 robot_id";
            res.status = 400;
            res.set_content(error.dump(), "application/json");
            return;
          }
        }
      } else {
        robot_id = GenerateUUID();
      }

      // 添加到数据库
      bool success = config_db_->AddRobot(robot_id, robot_name, serial_number, enabled);
      if (success) {
        // 添加到MQTT管理器
        mqtt_manager_->AddRobot(robot_id);

        json response;
        response["success"] = true;
        response["message"] = "机器人添加成功";
        response["robot_id"] = robot_id;
        res.set_content(response.dump(), "application/json");
        LOG(INFO) << "API: 添加机器人成功 - " << robot_id << " (" << robot_name << ")";
      } else {
        json error;
        error["success"] = false;
        error["error"] = "添加机器人到数据库失败";
        res.status = 500;
        res.set_content(error.dump(), "application/json");
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "添加机器人失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
  });

  // API: 删除机器人
  svr.Post("/api/v1/robots/delete", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string robot_id = req.get_param_value("robot_id");

      // 从MQTT管理器移除
      mqtt_manager_->RemoveRobot(robot_id);

      // 从数据库删除
      bool success = config_db_->RemoveRobot(robot_id);
      if (success) {
        json response;
        response["success"] = true;
        response["message"] = "机器人删除成功";
        res.set_content(response.dump(), "application/json");
        LOG(INFO) << "API: 删除机器人成功 - " << robot_id;
      } else {
        json error;
        error["success"] = false;
        error["error"] = "从数据库删除机器人失败";
        res.status = 500;
        res.set_content(error.dump(), "application/json");
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "删除机器人失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // API: 编辑机器人信息（名称/ID/启用状态）
  svr.Post("/api/v1/robots/update", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string old_robot_id = req.get_param_value("robot_id");
      if (old_robot_id.empty()) {
        json error; error["success"] = false; error["error"] = "缺少 robot_id 参数";
        res.status = 400; res.set_content(error.dump(), "application/json"); return;
      }

      json body = json::parse(req.body);
      std::string new_robot_id  = body.value("robot_id",    old_robot_id);
      std::string new_robot_name = body.value("robot_name", "");
      bool new_enabled           = body.value("enabled",    true);

      // 如果 robot_id 要改变，检查新 ID 是否干冲
      if (new_robot_id != old_robot_id) {
        auto all = config_db_->GetAllRobots();
        for (const auto& r : all) {
          if (r.robot_id == new_robot_id) {
            json error; error["success"] = false;
            error["error"] = "robot_id \"" + new_robot_id + "\" 已存在";
            res.status = 400; res.set_content(error.dump(), "application/json"); return;
          }
        }
      }

      bool success = config_db_->UpdateRobotInfo(old_robot_id, new_robot_id, new_robot_name, new_enabled);
      if (!success) {
        json error; error["success"] = false; error["error"] = "数据库更新失败";
        res.status = 500; res.set_content(error.dump(), "application/json"); return;
      }

      // 同步 MqttManager
      if (new_robot_id != old_robot_id) {
        // ID 改变：移除旧的，如果启用则添加新的
        mqtt_manager_->RemoveRobot(old_robot_id);
        if (new_enabled) mqtt_manager_->AddRobot(new_robot_id);
      } else if (new_enabled) {
        mqtt_manager_->AddRobot(new_robot_id);
      } else {
        mqtt_manager_->RemoveRobot(new_robot_id);
      }

      json response;
      response["success"]    = true;
      response["message"]    = "机器人信息已更新";
      response["robot_id"]   = new_robot_id;
      response["robot_name"] = new_robot_name;
      response["enabled"]    = new_enabled;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 编辑机器人 - " << old_robot_id << " -> " << new_robot_id;
    } catch (const std::exception& e) {
      LOG(ERROR) << "编辑机器人失败: " << e.what();
      json error; error["success"] = false; error["error"] = e.what();
      res.status = 400; res.set_content(error.dump(), "application/json");
    }
  });

  // API: 批量添加机器人
  svr.Post("/api/v1/robots/batch_add", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      json body = json::parse(req.body);

      if (!body.contains("robots") || !body["robots"].is_array()) {
        json error;
        error["success"] = false;
        error["error"] = "缺少robots数组参数";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      std::vector<ConfigDb::RobotInfo> robots;
      for (const auto& robot_json : body["robots"]) {
        ConfigDb::RobotInfo info;
        info.robot_id = GenerateUUID();  // 自动生成UUID
        info.robot_name = robot_json.value("robot_name", "");
        info.serial_number = robot_json.value("serial_number", 0);
        info.enabled = robot_json.value("enabled", true);
        robots.push_back(info);
      }

      // 检查序号是否有重复或已存在
      std::set<int> serial_numbers;
      for (const auto& robot : robots) {
        // 检查批量数据内部是否有重复序号
        if (serial_numbers.count(robot.serial_number) > 0) {
          json error;
          error["success"] = false;
          error["error"] = "批量数据中序号 " + std::to_string(robot.serial_number) + " 重复";
          res.status = 400;
          res.set_content(error.dump(), "application/json");
          return;
        }
        serial_numbers.insert(robot.serial_number);

        // 检查数据库中是否已存在该序号
        if (config_db_->IsSerialNumberExists(robot.serial_number)) {
          json error;
          error["success"] = false;
          error["error"] = "序号 " + std::to_string(robot.serial_number) + " 已存在，请使用其他序号";
          res.status = 400;
          res.set_content(error.dump(), "application/json");
          return;
        }
      }

      // 批量添加到数据库
      bool success = config_db_->AddRobotsBatch(robots);
      if (success) {
        // 将启用的机器人添加到MQTT管理器
        for (const auto& robot : robots) {
          if (robot.enabled) {
            mqtt_manager_->AddRobot(robot.robot_id);
          }
        }

        json response;
        response["success"] = true;
        response["message"] = "批量添加成功";
        response["count"] = robots.size();
        res.set_content(response.dump(), "application/json");
        LOG(INFO) << "API: 批量添加机器人成功, 数量: " << robots.size();
      } else {
        json error;
        error["success"] = false;
        error["error"] = "批量添加机器人失败";
        res.status = 500;
        res.set_content(error.dump(), "application/json");
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "批量添加机器人失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
  });

  // API: 批量删除机器人
  svr.Post("/api/v1/robots/batch_delete", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      json body = json::parse(req.body);

      if (!body.contains("robot_ids") || !body["robot_ids"].is_array()) {
        json error;
        error["success"] = false;
        error["error"] = "缺少robot_ids数组参数";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      std::vector<std::string> robot_ids;
      for (const auto& id : body["robot_ids"]) {
        robot_ids.push_back(id.get<std::string>());
      }

      // 从MQTT管理器批量移除
      for (const auto& robot_id : robot_ids) {
        mqtt_manager_->RemoveRobot(robot_id);
      }

      // 从数据库批量删除
      bool success = config_db_->RemoveRobotsBatch(robot_ids);
      if (success) {
        json response;
        response["success"] = true;
        response["message"] = "批量删除成功";
        response["count"] = robot_ids.size();
        res.set_content(response.dump(), "application/json");
        LOG(INFO) << "API: 批量删除机器人成功, 数量: " << robot_ids.size();
      } else {
        json error;
        error["success"] = false;
        error["error"] = "批量删除机器人失败";
        res.status = 500;
        res.set_content(error.dump(), "application/json");
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "批量删除机器人失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 400;
      res.set_content(error.dump(), "application/json");
    }
  });

  // API: 获取机器人详细数据
  svr.Get("/api/v1/robots/data", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string robot_id = req.get_param_value("robot_id");

      auto robot = mqtt_manager_->GetRobot(robot_id);

      if (robot) {
        json robot_data;
        robot_data["robot_id"] = robot->GetId();
        robot_data["status"] = robot->IsRunning() ? "running" : "stopped";
        robot_data["last_data"] = robot->GetLastData();
        robot_data["software_version"] = robot->GetData().software_version;
        const auto& d = robot->GetData();
        robot_data["fault_status"] = (d.alarm_fa != 0 || d.alarm_fb != 0 || d.alarm_fc != 0 || d.alarm_fd != 0);

        // 从数据库获取serial_number和robot_name
        auto all_robots = config_db_->GetAllRobots();
        for (const auto& r : all_robots) {
          if (r.robot_id == robot_id) {
            robot_data["serial_number"] = r.serial_number;
            robot_data["robot_name"] = r.robot_name;
            break;
          }
        }

        res.set_content(robot_data.dump(), "application/json");
        LOG(INFO) << "API: 获取机器人数据 - " << robot_id;
      } else {
        // 机器人不在 MqttManager（可能已禁用）- 尝试从数据库读取快照
        auto all_robots = config_db_->GetAllRobots();
        const ConfigDb::RobotInfo* found_info = nullptr;
        for (const auto& r : all_robots) {
          if (r.robot_id == robot_id) { found_info = &r; break; }
        }

        if (found_info) {
          json robot_data;
          robot_data["robot_id"] = robot_id;
          robot_data["serial_number"] = found_info->serial_number;
          robot_data["robot_name"] = found_info->robot_name;
          robot_data["status"] = "stopped";

          // 读取数据库快照，构造与 GetLastData() 兼容的 last_data 结构
          std::string snapshot = config_db_->GetRobotDataSnapshot(robot_id);
          json last_data_json;
          last_data_json["robot_id"] = robot_id;
          last_data_json["running"] = false;
          try {
            last_data_json["data"] = snapshot.empty() ? json::object() : json::parse(snapshot);
          } catch (...) {
            last_data_json["data"] = json::object();
          }
          robot_data["last_data"] = last_data_json.dump();

          // 从快照提取 software_version
          std::string sw_ver;
          try {
            if (!snapshot.empty()) {
              sw_ver = json::parse(snapshot).value("software_version", "");
            }
          } catch (...) {}
          robot_data["software_version"] = sw_ver;

          // 从数据库读取告警状态
          auto alarms = config_db_->GetRobotAlarms(robot_id);
          robot_data["fault_status"] = (alarms.alarm_fa != 0 || alarms.alarm_fb != 0 ||
                                        alarms.alarm_fc != 0 || alarms.alarm_fd != 0);

          res.set_content(robot_data.dump(), "application/json");
          LOG(INFO) << "API: 获取机器人数据(已禁用) - " << robot_id;
        } else {
          json error;
          error["success"] = false;
          error["error"] = "机器人不存在";
          res.status = 404;
          res.set_content(error.dump(), "application/json");
        }
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "获取机器人数据失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // API: 获取机器人告警
  svr.Get("/api/v1/robots/get_alarms", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string robot_id = req.get_param_value("robot_id");

      // 从robot对象获取告警
      auto robot = mqtt_manager_->GetRobot(robot_id);
      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      json response;
      response["success"] = true;
      response["robot_id"] = robot_id;
      response["alarm_fa"] = robot->GetData().alarm_fa;
      response["alarm_fb"] = robot->GetData().alarm_fb;
      response["alarm_fc"] = robot->GetData().alarm_fc;
      response["alarm_fd"] = robot->GetData().alarm_fd;
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
      LOG(ERROR) << "获取机器人告警失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // API: 设置机器人告警
  svr.Post("/api/v1/robots/set_alarms", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string robot_id = req.get_param_value("robot_id");
      json body = json::parse(req.body);

      auto robot = mqtt_manager_->GetRobot(robot_id);

      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      // 更新告警值
      if (body.contains("alarm_fa")) {
        robot->GetData().alarm_fa = body["alarm_fa"].get<uint32_t>();
      }
      if (body.contains("alarm_fb")) {
        robot->GetData().alarm_fb = body["alarm_fb"].get<uint16_t>();
      }
      if (body.contains("alarm_fc")) {
        robot->GetData().alarm_fc = body["alarm_fc"].get<uint32_t>();
      }
      if (body.contains("alarm_fd")) {
        robot->GetData().alarm_fd = body["alarm_fd"].get<uint16_t>();
      }

      // 通过Robot统一接口更新告警到数据库
      robot->UpdateAlarmsToDb();

      json response;
      response["success"] = true;
      response["message"] = "告警设置成功";
      response["robot_id"] = robot_id;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 设置机器人告警 - " << robot_id;
    } catch (const std::exception& e) {
      LOG(ERROR) << "设置机器人告警失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/motor_params - 发送电机参数设置请求
  svr.Post("/api/v1/robots/motor_params", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    try {
      json body = json::parse(req.body);

      const std::vector<std::string> required_fields = {
          "walk_motor_speed",
          "brush_motor_speed",
          "windproof_motor_speed",
          "walk_motor_max_current_ma",
          "brush_motor_max_current_ma",
          "windproof_motor_max_current_ma",
          "walk_motor_warning_current_ma",
          "brush_motor_warning_current_ma",
          "windproof_motor_warning_current_ma",
          "walk_motor_mileage_m",
          "brush_motor_timeout_s",
          "windproof_motor_timeout_s",
          "reverse_time_s",
          "protection_angle"};

      for (const auto& field : required_fields) {
        if (!body.contains(field)) {
          json error;
          error["success"] = false;
          error["error"] = "缺少必需参数: " + field;
          res.status = 400;
          res.set_content(error.dump(), "application/json");
          return;
        }
      }

      auto robot = mqtt_manager_->GetRobot(robot_id);
      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      robot->SendMotorParamsRequest(
          static_cast<uint8_t>(body["walk_motor_speed"].get<int>()),
          static_cast<uint8_t>(body["brush_motor_speed"].get<int>()),
          static_cast<uint8_t>(body["windproof_motor_speed"].get<int>()),
          static_cast<uint16_t>(body["walk_motor_max_current_ma"].get<int>()),
          static_cast<uint16_t>(body["brush_motor_max_current_ma"].get<int>()),
          static_cast<uint16_t>(body["windproof_motor_max_current_ma"].get<int>()),
          static_cast<uint16_t>(body["walk_motor_warning_current_ma"].get<int>()),
          static_cast<uint16_t>(body["brush_motor_warning_current_ma"].get<int>()),
          static_cast<uint16_t>(body["windproof_motor_warning_current_ma"].get<int>()),
          static_cast<uint16_t>(body["walk_motor_mileage_m"].get<int>()),
          static_cast<uint16_t>(body["brush_motor_timeout_s"].get<int>()),
          static_cast<uint16_t>(body["windproof_motor_timeout_s"].get<int>()),
          static_cast<uint8_t>(body["reverse_time_s"].get<int>()),
          static_cast<uint8_t>(body["protection_angle"].get<int>()));

      // 同步到内存 data_ 并持久化到数据库
      {
        auto& mp = robot->GetData().motor_params;
        mp.walk_motor_speed                   = static_cast<uint8_t>(body["walk_motor_speed"].get<int>());
        mp.brush_motor_speed                  = static_cast<uint8_t>(body["brush_motor_speed"].get<int>());
        mp.windproof_motor_speed              = static_cast<uint8_t>(body["windproof_motor_speed"].get<int>());
        mp.walk_motor_max_current_ma          = static_cast<uint16_t>(body["walk_motor_max_current_ma"].get<int>());
        mp.brush_motor_max_current_ma         = static_cast<uint16_t>(body["brush_motor_max_current_ma"].get<int>());
        mp.windproof_motor_max_current_ma     = static_cast<uint16_t>(body["windproof_motor_max_current_ma"].get<int>());
        mp.walk_motor_warning_current_ma      = static_cast<uint16_t>(body["walk_motor_warning_current_ma"].get<int>());
        mp.brush_motor_warning_current_ma     = static_cast<uint16_t>(body["brush_motor_warning_current_ma"].get<int>());
        mp.windproof_motor_warning_current_ma = static_cast<uint16_t>(body["windproof_motor_warning_current_ma"].get<int>());
        mp.walk_motor_mileage_m               = static_cast<uint16_t>(body["walk_motor_mileage_m"].get<int>());
        mp.brush_motor_timeout_s              = static_cast<uint16_t>(body["brush_motor_timeout_s"].get<int>());
        mp.windproof_motor_timeout_s          = static_cast<uint16_t>(body["windproof_motor_timeout_s"].get<int>());
        mp.reverse_time_s                     = static_cast<uint8_t>(body["reverse_time_s"].get<int>());
        mp.protection_angle                   = static_cast<uint8_t>(body["protection_angle"].get<int>());
        config_db_->UpdateRobotDataSnapshot(robot_id, robot->SerializeDataSnapshot());
      }

      json response;
      response["success"] = true;
      response["message"] = "电机参数设置请求已发送";
      response["robot_id"] = robot_id;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 发送电机参数设置请求 - 机器人: " << robot_id;
    } catch (const std::exception& e) {
      LOG(ERROR) << "发送电机参数设置请求失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  svr.Post("/api/v1/robots/battery_params", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    try {
      json body = json::parse(req.body);

      const std::vector<std::string> required_fields = {
          "protection_current_ma", "high_temp_threshold", "low_temp_threshold",
          "protection_temp",      "recovery_temp",      "protection_voltage",
          "recovery_voltage",     "protection_battery_level",
          "limit_run_battery_level", "recovery_battery_level"};

      for (const auto& field : required_fields) {
        if (!body.contains(field)) {
          json error;
          error["success"] = false;
          error["error"] = "缺少必需参数: " + field;
          res.status = 400;
          res.set_content(error.dump(), "application/json");
          return;
        }
      }

      auto robot = mqtt_manager_->GetRobot(robot_id);
      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      robot->SendBatteryParamsRequest(
          static_cast<uint16_t>(body["protection_current_ma"].get<int>()),
          static_cast<uint8_t>(body["high_temp_threshold"].get<int>()),
          static_cast<uint8_t>(body["low_temp_threshold"].get<int>()),
          static_cast<uint8_t>(body["protection_temp"].get<int>()),
          static_cast<uint8_t>(body["recovery_temp"].get<int>()),
          static_cast<uint8_t>(body["protection_voltage"].get<int>()),
          static_cast<uint8_t>(body["recovery_voltage"].get<int>()),
          static_cast<uint8_t>(body["protection_battery_level"].get<int>()),
          static_cast<uint8_t>(body["limit_run_battery_level"].get<int>()),
          static_cast<uint8_t>(body["recovery_battery_level"].get<int>()));

      // 同步到内存 data_ 并持久化到数据库
      {
        auto& tv = robot->GetData().temp_voltage_protection;
        tv.protection_current_ma    = static_cast<uint16_t>(body["protection_current_ma"].get<int>());
        tv.high_temp_threshold      = static_cast<uint8_t>(body["high_temp_threshold"].get<int>());
        tv.low_temp_threshold       = static_cast<uint8_t>(body["low_temp_threshold"].get<int>());
        tv.protection_temp          = static_cast<uint8_t>(body["protection_temp"].get<int>());
        tv.recovery_temp            = static_cast<uint8_t>(body["recovery_temp"].get<int>());
        tv.protection_voltage       = static_cast<uint8_t>(body["protection_voltage"].get<int>());
        tv.recovery_voltage         = static_cast<uint8_t>(body["recovery_voltage"].get<int>());
        tv.protection_battery_level = static_cast<uint8_t>(body["protection_battery_level"].get<int>());
        tv.limit_run_battery_level  = static_cast<uint8_t>(body["limit_run_battery_level"].get<int>());
        tv.recovery_battery_level   = static_cast<uint8_t>(body["recovery_battery_level"].get<int>());
        config_db_->UpdateRobotDataSnapshot(robot_id, robot->SerializeDataSnapshot());
      }

      json response;
      response["success"] = true;
      response["message"] = "电池参数设置请求已发送";
      response["robot_id"] = robot_id;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 发送电池参数设置请求 - 机器人: " << robot_id;
    } catch (const std::exception& e) {
      LOG(ERROR) << "发送电池参数设置请求失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  svr.Post("/api/v1/robots/schedule_params", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    try {
      json body = json::parse(req.body);

      if (!body.contains("tasks") || !body["tasks"].is_array()) {
        json error;
        error["success"] = false;
        error["error"] = "缺少必需参数: tasks(数组)";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      const auto& tasks_json = body["tasks"];
      if (tasks_json.size() != 7) {
        json error;
        error["success"] = false;
        error["error"] = "tasks 数组长度必须为7";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      std::vector<ScheduleTask> tasks;
      tasks.reserve(7);
      for (size_t i = 0; i < 7; ++i) {
        const auto& item = tasks_json[i];
        if (!item.contains("weekday") || !item.contains("hour") ||
            !item.contains("minute") || !item.contains("run_count")) {
          json error;
          error["success"] = false;
          error["error"] = "tasks[" + std::to_string(i) + "] 缺少必需参数";
          res.status = 400;
          res.set_content(error.dump(), "application/json");
          return;
        }

        ScheduleTask task;
        task.weekday = item["weekday"].get<int>();
        task.hour = item["hour"].get<int>();
        task.minute = item["minute"].get<int>();
        task.run_count = item["run_count"].get<int>();
        tasks.push_back(task);
      }

      auto robot = mqtt_manager_->GetRobot(robot_id);
      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      robot->SendScheduleParamsRequest(tasks);

      // 同步到内存 data_ 并持久化到数据库
      robot->GetData().schedule_tasks = tasks;
      config_db_->UpdateRobotDataSnapshot(robot_id, robot->SerializeDataSnapshot());

      json response;
      response["success"] = true;
      response["message"] = "定时设置请求已发送";
      response["robot_id"] = robot_id;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 发送定时设置请求 - 机器人: " << robot_id;
    } catch (const std::exception& e) {
      LOG(ERROR) << "发送定时设置请求失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  svr.Post("/api/v1/robots/parking_position", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    try {
      json body = json::parse(req.body);
      if (!body.contains("parking_position")) {
        json error;
        error["success"] = false;
        error["error"] = "缺少必需参数: parking_position";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      auto robot = mqtt_manager_->GetRobot(robot_id);
      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      uint8_t parking_position = static_cast<uint8_t>(body["parking_position"].get<int>());
      robot->SendParkingPositionRequest(parking_position);

      // 同步到内存 data_ 并持久化到数据库
      robot->GetData().parking_position = parking_position;
      config_db_->UpdateRobotDataSnapshot(robot_id, robot->SerializeDataSnapshot());

      json response;
      response["success"] = true;
      response["message"] = "停机位设置请求已发送";
      response["robot_id"] = robot_id;
      response["parking_position"] = parking_position;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 发送停机位设置请求 - 机器人: " << robot_id;
    } catch (const std::exception& e) {
      LOG(ERROR) << "发送停机位设置请求失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/lora_params - 设置Lora参数（功率/频率/速率）
  svr.Post("/api/v1/robots/lora_params", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    try {
      json body = json::parse(req.body);
      if (!body.contains("power") || !body.contains("frequency") || !body.contains("rate")) {
        json error;
        error["success"] = false;
        error["error"] = "缺少必需参数: power, frequency, rate";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      auto robot = mqtt_manager_->GetRobot(robot_id);
      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      robot->GetData().lora_params.power     = body["power"].get<int>();
      robot->GetData().lora_params.frequency = body["frequency"].get<int>();
      robot->GetData().lora_params.rate      = body["rate"].get<int>();
      robot->SendLoraAndCleanSettingsReport();
      config_db_->UpdateRobotDataSnapshot(robot_id, robot->SerializeDataSnapshot());

      json response;
      response["success"] = true;
      response["message"] = "Lora参数设置已发送";
      response["robot_id"] = robot_id;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 设置Lora参数 - 机器人: " << robot_id;
    } catch (const std::exception& e) {
      LOG(ERROR) << "设置Lora参数失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/daytime_scan_protect - 设置白天防误扫开关
  svr.Post("/api/v1/robots/daytime_scan_protect", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    try {
      json body = json::parse(req.body);
      if (!body.contains("enabled")) {
        json error;
        error["success"] = false;
        error["error"] = "缺少必需参数: enabled";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      auto robot = mqtt_manager_->GetRobot(robot_id);
      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      robot->GetData().daytime_scan_protect = body["enabled"].get<bool>();
      robot->SendLoraAndCleanSettingsReport();
      config_db_->UpdateRobotDataSnapshot(robot_id, robot->SerializeDataSnapshot());

      json response;
      response["success"] = true;
      response["message"] = "白天防误扫设置已发送";
      response["robot_id"] = robot_id;
      response["enabled"] = robot->GetData().daytime_scan_protect;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 设置白天防误扫 - 机器人: " << robot_id;
    } catch (const std::exception& e) {
      LOG(ERROR) << "设置白天防误扫失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/data - 设置机器人运行数据（E4上报字段），仅更新UI模拟数据，不触发MQTT
  svr.Post("/api/v1/robots/data", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    try {
      json body = json::parse(req.body);

      auto robot = mqtt_manager_->GetRobot(robot_id);
      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      auto& d = robot->GetData();
      if (body.contains("main_motor_current"))  d.main_motor_current  = body["main_motor_current"].get<int>();
      if (body.contains("slave_motor_current")) d.slave_motor_current = body["slave_motor_current"].get<int>();
      if (body.contains("battery_voltage"))     d.battery_voltage     = body["battery_voltage"].get<int>();
      if (body.contains("battery_current"))     d.battery_current     = body["battery_current"].get<int>();
      if (body.contains("battery_status"))      d.battery_status      = body["battery_status"].get<int>();
      if (body.contains("battery_level"))       d.battery_level       = body["battery_level"].get<int>();
      if (body.contains("battery_temperature")) d.battery_temperature = body["battery_temperature"].get<int>();
      if (body.contains("position"))            d.position            = body["position"].get<int>();
      if (body.contains("working_duration"))    d.working_duration    = body["working_duration"].get<int>();
      if (body.contains("solar_voltage"))       d.solar_voltage       = body["solar_voltage"].get<int>();
      if (body.contains("solar_current"))       d.solar_current       = body["solar_current"].get<int>();
      if (body.contains("total_run_count"))     d.total_run_count     = body["total_run_count"].get<int>();
      if (body.contains("current_lap_count"))   d.current_lap_count   = body["current_lap_count"].get<int>();
      if (body.contains("board_temperature"))   d.board_temperature   = body["board_temperature"].get<int>();
      if (body.contains("alarm_fa"))            d.alarm_fa            = body["alarm_fa"].get<int>();
      if (body.contains("alarm_fb"))            d.alarm_fb            = body["alarm_fb"].get<int>();
      if (body.contains("alarm_fc"))            d.alarm_fc            = body["alarm_fc"].get<int>();
      if (body.contains("alarm_fd"))            d.alarm_fd            = body["alarm_fd"].get<int>();

      config_db_->UpdateRobotDataSnapshot(robot_id, robot->SerializeDataSnapshot());

      json response;
      response["success"] = true;
      response["message"] = "机器人运行数据已更新并持久化";
      response["robot_id"] = robot_id;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 设置机器人运行数据(E4) - 机器人: " << robot_id;
    } catch (const std::exception& e) {
      LOG(ERROR) << "设置机器人运行数据失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/schedule_start - 发送定时启动请求
  svr.Post("/api/v1/robots/schedule_start", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    try {
      // 解析请求体
      json body = json::parse(req.body);

      if (!body.contains("schedule_id") || !body.contains("weekday") ||
          !body.contains("hour") || !body.contains("minute") || !body.contains("run_count")) {
        json error;
        error["success"] = false;
        error["error"] = "缺少必需参数: schedule_id, weekday, hour, minute, run_count";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      uint8_t schedule_id = body["schedule_id"].get<int>();
      uint8_t weekday = body["weekday"].get<int>();
      uint8_t hour = body["hour"].get<int>();
      uint8_t minute = body["minute"].get<int>();
      uint8_t run_count = body["run_count"].get<int>();

      // 查找机器人
      auto robot = mqtt_manager_->GetRobot(robot_id);

      if (robot) {
        // 发送定时启动请求
        robot->SendScheduleStartRequest(schedule_id, weekday, hour, minute, run_count);

        json response;
        response["success"] = true;
        response["message"] = "定时启动请求已发送";
        response["robot_id"] = robot_id;
        response["schedule_id"] = schedule_id;
        response["weekday"] = weekday;
        response["hour"] = hour;
        response["minute"] = minute;
        response["run_count"] = run_count;

        res.set_content(response.dump(), "application/json");
        LOG(INFO) << "API: 发送定时启动请求 - 机器人: " << robot_id;
      } else {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "发送定时启动请求失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/start - 发送启动请求
  svr.Post("/api/v1/robots/start", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    try {
      // 查找机器人
      auto robot = mqtt_manager_->GetRobot(robot_id);

      if (robot) {
        // 发送启动请求
        robot->SendStartRequest();

        json response;
        response["success"] = true;
        response["message"] = "启动请求已发送";
        response["robot_id"] = robot_id;

        res.set_content(response.dump(), "application/json");
        LOG(INFO) << "API: 发送启动请求 - 机器人: " << robot_id;
      } else {
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "发送启动请求失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/time_sync - 发送校时请求
  svr.Post("/api/v1/robots/time_sync", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    LOG(INFO) << "收到校时请求 - 机器人: " << robot_id;

    try {
      auto robot = mqtt_manager_->GetRobot(robot_id);

      if (!robot) {
        LOG(WARNING) << "未找到机器人: " << robot_id;
        json error;
        error["success"] = false;
        error["error"] = "未找到机器人";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      // 发送校时请求
      robot->SendTimeSyncRequest();

      json response;
      response["success"] = true;
      response["message"] = "校时请求已发送";
      response["robot_id"] = robot->GetId();
      res.set_content(response.dump(), "application/json");

      LOG(INFO) << "校时请求已发送 - 机器人: " << robot->GetId();

    } catch (const std::exception& e) {
      LOG(ERROR) << "发送校时请求失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/lora_clean_settings - 发送Lora参数&清扫设置上报
  svr.Post("/api/v1/robots/lora_clean_settings", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    LOG(INFO) << "收到Lora参数&清扫设置上报请求 - 机器人: " << robot_id;

    try {
      auto robot = mqtt_manager_->GetRobot(robot_id);

      if (!robot) {
        LOG(WARNING) << "未找到机器人: " << robot_id;
        json error;
        error["success"] = false;
        error["error"] = "未找到机器人";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      // 发送Lora参数&清扫设置上报
      robot->SendLoraAndCleanSettingsReport();

      json response;
      response["success"] = true;
      response["message"] = "Lora参数&清扫设置上报已发送";
      response["robot_id"] = robot->GetId();
      res.set_content(response.dump(), "application/json");

      LOG(INFO) << "Lora参数&清扫设置上报已发送 - 机器人: " << robot->GetId();

    } catch (const std::exception& e) {
      LOG(ERROR) << "发送Lora参数&清扫设置上报失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/robot_data - 发送机器人数据上报
  svr.Post("/api/v1/robots/robot_data", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");

    LOG(INFO) << "收到机器人数据上报请求 - 机器人: " << robot_id;

    try {
      auto robot = mqtt_manager_->GetRobot(robot_id);

      if (!robot) {
        LOG(WARNING) << "未找到机器人: " << robot_id;
        json error;
        error["success"] = false;
        error["error"] = "未找到机器人";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      // 发送机器人数据上报
      robot->SendRobotDataReport();

      json response;
      response["success"] = true;
      response["message"] = "机器人数据上报已发送";
      response["robot_id"] = robot->GetId();
      res.set_content(response.dump(), "application/json");

      LOG(INFO) << "机器人数据上报已发送 - 机器人: " << robot->GetId();

    } catch (const std::exception& e) {
      LOG(ERROR) << "发送机器人数据上报失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/robots/trigger_report - 手动触发指定上报指令（E0~E9）
  svr.Post("/api/v1/robots/trigger_report", [this](const httplib::Request& req, httplib::Response& res) {
    std::string robot_id = req.get_param_value("robot_id");
    std::string code_str = req.get_param_value("code");  // 如 "E0", "E1" ...

    LOG(INFO) << "收到手动触发上报请求 - 机器人: " << robot_id << ", 指令: " << code_str;

    try {
      if (robot_id.empty() || code_str.empty()) {
        json error;
        error["success"] = false;
        error["error"] = "缺少必要参数 robot_id 或 code";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      auto robot = mqtt_manager_->GetRobot(robot_id);
      if (!robot) {
        json error;
        error["success"] = false;
        error["error"] = "未找到机器人: " + robot_id;
        res.status = 404;
        res.set_content(error.dump(), "application/json");
        return;
      }

      // 将 code 字符串转换为字节值（支持 "E0"~"E9" 或 "0xE0"~"0xE9"）
      std::string hex = code_str;
      if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) hex = hex.substr(2);
      uint8_t code = static_cast<uint8_t>(std::stoul(hex, nullptr, 16));

      std::string desc;
      switch (code) {
        case 0xE0: robot->SendLoraAndCleanSettingsReport(); desc = "Lora参数&清扫设置"; break;
        case 0xE1: robot->SendMotorParamsReport();          desc = "电机和电池参数";   break;
        case 0xE4: robot->SendRobotDataReport();            desc = "机器人数据";       break;
        case 0xE9: robot->SendCleanRecordReport();          desc = "清扫记录";         break;
        case 0xE5: robot->SendCurrentDataReport();       desc = "电流数据";           break;
        case 0xE6: robot->SendScheduledNotRunReport();   desc = "定时请求未运行原因"; break;
        case 0xE7: robot->SendNotStartedReport();          desc = "未启动原因";       break;
        case 0xE8: robot->SendStartupConfirmReport();      desc = "启动请求回复确认"; break;
        default: {
          json error;
          error["success"] = false;
          error["error"] = "不支持的上报指令: " + code_str;
          res.status = 400;
          res.set_content(error.dump(), "application/json");
          return;
        }
      }

      json response;
      response["success"] = true;
      response["message"] = desc + " 上报已发送";
      response["robot_id"] = robot_id;
      response["code"] = code_str;
      res.set_content(response.dump(), "application/json");

      LOG(INFO) << "手动触发上报完成 - 机器人: " << robot_id << ", 指令: " << desc;
    } catch (const std::exception& e) {
      LOG(ERROR) << "手动触发上报失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // ─── 系统配置：定时上报间隔 ─────────────────────────────────────────────

  // GET /api/v1/system/version - 获取后端版本号
  svr.Get("/api/v1/system/version", [](const httplib::Request&, httplib::Response& res) {
    json response;
    response["success"] = true;
    response["version"] = APP_VERSION_STR;
    response["major"]   = APP_VERSION_MAJOR;
    response["minor"]   = APP_VERSION_MINOR;
    response["patch"]   = APP_VERSION_PATCH;
    res.set_content(response.dump(), "application/json");
  });

  // GET /api/v1/system/report_intervals - 获取三类上报间隔配置
  svr.Get("/api/v1/system/report_intervals", [this](const httplib::Request&, httplib::Response& res) {
    try {
      json response;
      response["success"] = true;
      response["robot_data_report_interval"]   = config_db_->GetIntValue("robot_data_report_interval", 600);
      response["motor_params_report_interval"] = config_db_->GetIntValue("motor_params_report_interval", 3600);
      response["lora_clean_report_interval"]   = config_db_->GetIntValue("lora_clean_report_interval", 3600);
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/system/report_intervals - 更新三类上报间隔配置（并实时生效）
  svr.Post("/api/v1/system/report_intervals", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      json body = json::parse(req.body);

      int robot_data_s   = body.value("robot_data_report_interval",   config_db_->GetIntValue("robot_data_report_interval",   600));
      int motor_params_s = body.value("motor_params_report_interval", config_db_->GetIntValue("motor_params_report_interval", 3600));
      int lora_clean_s   = body.value("lora_clean_report_interval",   config_db_->GetIntValue("lora_clean_report_interval",   3600));

      if (robot_data_s < 10 || motor_params_s < 10 || lora_clean_s < 10) {
        json error;
        error["success"] = false;
        error["error"] = "间隔最小值为10秒";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      bool ok = config_db_->SetValue("robot_data_report_interval",   std::to_string(robot_data_s))
             && config_db_->SetValue("motor_params_report_interval", std::to_string(motor_params_s))
             && config_db_->SetValue("lora_clean_report_interval",   std::to_string(lora_clean_s));

      if (!ok) {
        json error;
        error["success"] = false;
        error["error"] = "数据库写入失败";
        res.status = 500;
        res.set_content(error.dump(), "application/json");
        return;
      }

      // 实时应用到所有已运行的机器人
      mqtt_manager_->UpdateAllRobotsReportIntervals(robot_data_s, motor_params_s, lora_clean_s);

      json response;
      response["success"] = true;
      response["message"] = "上报间隔已更新并实时生效";
      response["robot_data_report_interval"]   = robot_data_s;
      response["motor_params_report_interval"] = motor_params_s;
      response["lora_clean_report_interval"]   = lora_clean_s;
      res.set_content(response.dump(), "application/json");

      LOG(INFO) << "上报间隔已更新 - 机器人数据:" << robot_data_s
                << "s, 电机参数:" << motor_params_s << "s, Lora&清扫:" << lora_clean_s << "s";
    } catch (const std::exception& e) {
      LOG(ERROR) << "更新上报间隔失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // ─── MQTT 服务配置 ─────────────────────────────────────────────────────────

  // GET /api/v1/system/mqtt_config - 获取 MQTT 服务地址、用户名及连接状态
  svr.Get("/api/v1/system/mqtt_config", [this](const httplib::Request&, httplib::Response& res) {
    try {
      json response;
      response["success"]   = true;
      response["broker"]    = mqtt_manager_->GetBroker();
      response["username"]  = mqtt_manager_->GetUsername();
      response["connected"] = mqtt_manager_->IsConnected();
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // POST /api/v1/system/mqtt_config - 更新 MQTT 服务配置并重新连接
  svr.Post("/api/v1/system/mqtt_config", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      json body = json::parse(req.body);
      if (!body.contains("broker") || body["broker"].get<std::string>().empty()) {
        json error;
        error["success"] = false;
        error["error"] = "缺少必填参数: broker"
; res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      std::string broker   = body["broker"].get<std::string>();
      std::string username = body.value("username", "");
      std::string password = body.value("password", "");

      bool ok = mqtt_manager_->ReconfigureAndReconnect(broker, username, password);

      json response;
      response["success"]   = ok;
      response["message"]   = ok ? "MQTT 服务配置已更新并重新连接" : "MQTT 重连失败，配置已保存";
      response["broker"]    = mqtt_manager_->GetBroker();
      response["username"]  = mqtt_manager_->GetUsername();
      response["connected"] = mqtt_manager_->IsConnected();
      if (!ok) res.status = 500;
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 更新 MQTT 配置 - broker: " << broker
                << ", user: " << username << ", 连接: " << (ok ? "ok" : "fail");
    } catch (const std::exception& e) {
      LOG(ERROR) << "MQTT 配置更新失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // GET /api/v1/system/firmware - 列出固件目录中的升级包文件
  svr.Get("/api/v1/system/firmware", [this](const httplib::Request&, httplib::Response& res) {
    namespace fs = std::filesystem;
    std::string firmware_dir = config_db_->GetValue("firmware_dir", "./firmware");
    json result;
    result["success"] = true;
    result["version"] = config_db_->GetValue("robot_version", "");
    json files_arr = json::array();
    try {
      if (!fs::exists(firmware_dir)) {
        fs::create_directories(firmware_dir);
        LOG(INFO) << "固件目录不存在，已自动创建: " << firmware_dir;
      }
      if (fs::is_directory(firmware_dir)) {
        for (const auto& entry : fs::directory_iterator(firmware_dir)) {
          if (!entry.is_regular_file()) continue;
          if (entry.path().extension().string() != ".bin") continue;
          json item;
          item["filename"] = entry.path().filename().string();
          item["size"]     = static_cast<uint64_t>(entry.file_size());
          files_arr.push_back(item);
        }
      }
    } catch (const std::exception& e) {
      LOG(WARNING) << "扫描固件目录失败: " << e.what();
    }
    result["files"] = files_arr;
    res.set_content(result.dump(), "application/json");
  });

  // POST /api/v1/system/robot_version - 设置机器人版本号
  svr.Post("/api/v1/system/robot_version", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      auto body = json::parse(req.body);
      std::string version = body.value("version", "");
      config_db_->SetValue("robot_version", version);
      json response;
      response["success"] = true;
      response["version"] = version;
      response["message"] = "机器人版本号已保存";
      res.set_content(response.dump(), "application/json");
      LOG(INFO) << "API: 设置机器人版本号: " << version;
    } catch (const std::exception& e) {
      res.status = 400;
      res.set_content(json{{"success", false}, {"error", e.what()}}.dump(), "application/json");
    }
  });

  // GET /api/v1/system/firmware/download?filename=xxx - 下载固件文件
  svr.Get("/api/v1/system/firmware/download", [this](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("filename")) {
      res.status = 400;
      res.set_content(json{{"success", false}, {"error", "缺少 filename 参数"}}.dump(), "application/json");
      return;
    }
    std::string filename = req.get_param_value("filename");
    if (filename.find("..") != std::string::npos ||
        filename.find('/') != std::string::npos  ||
        filename.find('\\') != std::string::npos) {
      res.status = 400;
      res.set_content(json{{"success", false}, {"error", "非法文件名"}}.dump(), "application/json");
      return;
    }
    std::string firmware_dir = config_db_->GetValue("firmware_dir", "./firmware");
    std::string filepath = firmware_dir + "/" + filename;
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
      res.status = 404;
      res.set_content(json{{"success", false}, {"error", "文件不存在: " + filename}}.dump(), "application/json");
      return;
    }
    auto sz = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::string content(static_cast<size_t>(sz), '\0');
    ifs.read(content.data(), sz);
    res.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    res.set_content(content, "application/octet-stream");
    LOG(INFO) << "API: 固件文件下载 - " << filename << " (" << sz << " bytes)";
  });

  LOG(INFO) << "HTTP服务器线程启动，监听端口: " << port_;

  // 启动服务器（阻塞）
  if (!svr.listen("0.0.0.0", port_)) {
    LOG(ERROR) << "HTTP服务器无法绑定到端口: " << port_;
    running_ = false;
  }
}
