#include "http_server.h"

#include <glog/logging.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <sstream>

#include "config_db.h"
#include "mqtt_manager.h"

// 使用cpp-httplib库
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

using json = nlohmann::json;

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
  svr.Get("/api/robots", [this](const httplib::Request&, httplib::Response& res) {
    try {
      auto robots = config_db_->GetAllRobots();
      json j = json::array();

      for (const auto& robot : robots) {
        json robot_json;
        robot_json["robot_id"] = robot.robot_id;
        robot_json["robot_name"] = robot.robot_name;
        robot_json["enabled"] = robot.enabled;
        j.push_back(robot_json);
      }

      res.set_content(j.dump(), "application/json");
      LOG(INFO) << "API: 获取机器人列表, 数量: " << robots.size();
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
      std::string robot_id = body["robot_id"];
      std::string robot_name = body.value("robot_name", "");

      if (robot_id.empty()) {
        json error;
        error["success"] = false;
        error["error"] = "robot_id不能为空";
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
      }

      // 添加到数据库
      bool success = config_db_->AddRobot(robot_id, robot_name, true);
      if (success) {
        // 添加到MQTT管理器
        mqtt_manager_->AddRobot(robot_id);

        json response;
        response["success"] = true;
        response["message"] = "机器人添加成功";
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

  // API: 获取机器人详细数据
  svr.Get(R"(/api/robots/([^/]+)/data)", [this](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string robot_id = req.matches[1];
      auto robot = mqtt_manager_->GetRobot(robot_id);

      if (robot) {
        json robot_data;
        robot_data["robot_id"] = robot->GetId();
        robot_data["status"] = robot->IsRunning() ? "运行中" : "已停止";
        robot_data["last_data"] = robot->GetLastData();

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

  LOG(INFO) << "HTTP服务器线程启动，监听端口: " << port_;

  // 启动服务器（阻塞）
  if (!svr.listen("0.0.0.0", port_)) {
    LOG(ERROR) << "HTTP服务器无法绑定到端口: " << port_;
    running_ = false;
  }
}
