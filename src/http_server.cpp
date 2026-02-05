#include "http_server.h"

#include <glog/logging.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <set>

#include "config_db.h"
#include "mqtt_manager.h"

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
    std::ifstream file("web/index.html");
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      res.set_content(buffer.str(), "text/html; charset=utf-8");
    } else {
      res.status = 404;
      res.set_content("web/index.html not found", "text/plain");
    }
  });

  // CSS样式文件
  svr.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
    std::ifstream file("web/style.css");
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      res.set_content(buffer.str(), "text/css; charset=utf-8");
    } else {
      res.status = 404;
      res.set_content("style.css not found", "text/plain");
    }
  });

  // JavaScript文件
  svr.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
    std::ifstream file("web/app.js");
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      res.set_content(buffer.str(), "application/javascript; charset=utf-8");
    } else {
      res.status = 404;
      res.set_content("app.js not found", "text/plain");
    }
  });

  // API: 获取所有机器人列表
  svr.Get("/api/robots", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      auto all_robots = config_db_->GetAllRobots();

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

      for (const auto& robot : all_robots) {
        if (robot.enabled) {
          enabled_count++;
        } else {
          disabled_count++;
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
        {"disabled", disabled_count}
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
  svr.Post("/api/robots", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      json body = json::parse(req.body);
      std::string robot_name = body.value("robot_name", "");
      int serial_number = body.value("serial_number", 0);

      if (serial_number <= 0) {
        json error;
        error["success"] = false;
        error["error"] = "序号必须大于0";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
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

      // 自动生成UUID作为robot_id
      std::string robot_id = GenerateUUID();

      // 添加到数据库
      bool success = config_db_->AddRobot(robot_id, robot_name, serial_number, true);
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
  svr.Delete(R"(/api/robots/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string robot_id = req.matches[1];

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

  // API: 更新机器人状态（启用/禁用）
  svr.Patch(R"(/api/robots/([^/]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string robot_id = req.matches[1];
      json body = json::parse(req.body);

      if (!body.contains("enabled")) {
        json error;
        error["success"] = false;
        error["error"] = "缺少enabled参数";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      bool enabled = body["enabled"];

      // 更新数据库中的状态
      bool success = config_db_->UpdateRobotStatus(robot_id, enabled);
      if (success) {
        // 根据状态添加或移除机器人
        if (enabled) {
          // 启用：添加到MQTT管理器
          mqtt_manager_->AddRobot(robot_id);
        } else {
          // 禁用：从MQTT管理器移除
          mqtt_manager_->RemoveRobot(robot_id);
        }

        json response;
        response["success"] = true;
        response["message"] = enabled ? "机器人已启用" : "机器人已禁用";
        res.set_content(response.dump(), "application/json");
        LOG(INFO) << "API: 更新机器人状态 - " << robot_id << " (" << (enabled ? "启用" : "禁用") << ")";
      } else {
        json error;
        error["success"] = false;
        error["error"] = "更新机器人状态失败";
        res.status = 500;
        res.set_content(error.dump(), "application/json");
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "更新机器人状态失败: " << e.what();
      json error;
      error["success"] = false;
      error["error"] = e.what();
      res.status = 500;
      res.set_content(error.dump(), "application/json");
    }
  });

  // API: 批量添加机器人
  svr.Post("/api/robots/batch", [this](const httplib::Request& req, httplib::Response& res) {
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
  svr.Post("/api/robots/batch-delete", [this](const httplib::Request& req, httplib::Response& res) {
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
  svr.Get(R"(/api/robots/([^/]+)/data)", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string robot_id = req.matches[1];
      auto robot = mqtt_manager_->GetRobot(robot_id);

      if (robot) {
        json robot_data;
        robot_data["robot_id"] = robot->GetId();
        robot_data["status"] = robot->IsRunning() ? "running" : "stopped";
        robot_data["last_data"] = robot->GetLastData();

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
        json error;
        error["success"] = false;
        error["error"] = "机器人不存在或未运行";
        res.status = 404;
        res.set_content(error.dump(), "application/json");
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

  // POST /api/robots/{id}/schedule_start - 发送定时启动请求
  svr.Post(R"(/api/robots/([^/]+)/schedule_start)", [this](const httplib::Request& req, httplib::Response& res) {
    std::string identifier = req.matches[1];

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

      // 判断是通过ID还是序号查找
      std::string robot_id = identifier;
      std::string type = req.get_param_value("type");

      if (type == "serial") {
        // 通过序号查找机器人ID
        int serial_number = std::stoi(identifier);
        robot_id = config_db_->GetRobotIdBySerial(serial_number);

        if (robot_id.empty()) {
          json error;
          error["success"] = false;
          error["error"] = "未找到序号为 " + identifier + " 的机器人";
          res.status = 404;
          res.set_content(error.dump(), "application/json");
          return;
        }
      }

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

  // POST /api/robots/{id}/start - 发送启动请求
  svr.Post(R"(/api/robots/([^/]+)/start)", [this](const httplib::Request& req, httplib::Response& res) {
    std::string identifier = req.matches[1];

    try {
      // 判断是通过ID还是序号查找
      std::string robot_id = identifier;
      std::string type = req.get_param_value("type");

      if (type == "serial") {
        // 通过序号查找机器人ID
        int serial_number = std::stoi(identifier);
        robot_id = config_db_->GetRobotIdBySerial(serial_number);

        if (robot_id.empty()) {
          json error;
          error["success"] = false;
          error["error"] = "未找到序号为 " + identifier + " 的机器人";
          res.status = 404;
          res.set_content(error.dump(), "application/json");
          return;
        }
      }

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

  // POST /api/robots/:id/time_sync - 发送校时请求
  svr.Post(R"(/api/robots/([^/]+)/time_sync)", [this](const httplib::Request& req, httplib::Response& res) {
    std::string identifier = req.matches[1];

    // 获取查询参数type（id或serial）
    std::string type = "id";  // 默认为id
    if (req.has_param("type")) {
      type = req.get_param_value("type");
    }

    LOG(INFO) << "收到校时请求 - 标识: " << identifier << ", 类型: " << type;

    try {
      std::shared_ptr<Robot> robot;

      if (type == "serial") {
        // 通过序号查找robot_id
        int serial_number = std::stoi(identifier);
        std::string robot_id = config_db_.GetRobotIdBySerial(serial_number);

        if (robot_id.empty()) {
          LOG(WARNING) << "未找到序号对应的机器人: " << serial_number;
          json error;
          error["success"] = false;
          error["error"] = "未找到序号对应的机器人";
          res.status = 404;
          res.set_content(error.dump(), "application/json");
          return;
        }

        robot = mqtt_manager_->GetRobot(robot_id);
      } else {
        // 直接使用robot_id
        robot = mqtt_manager_->GetRobot(identifier);
      }

      if (!robot) {
        LOG(WARNING) << "未找到机器人: " << identifier;
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

  LOG(INFO) << "HTTP服务器线程启动，监听端口: " << port_;

  // 启动服务器（阻塞）
  if (!svr.listen("0.0.0.0", port_)) {
    LOG(ERROR) << "HTTP服务器无法绑定到端口: " << port_;
    running_ = false;
  }
}
