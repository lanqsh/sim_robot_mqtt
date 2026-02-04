#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <atomic>
#include <memory>
#include <string>
#include <thread>

// 前向声明
class ConfigDb;
class MqttManager;

class HttpServer {
 private:
  std::shared_ptr<ConfigDb> config_db_;
  std::shared_ptr<MqttManager> mqtt_manager_;
  int port_;
  std::atomic<bool> running_;
  std::thread server_thread_;

  void ServerThreadFunc();

 public:
  HttpServer(std::shared_ptr<ConfigDb> config_db,
             std::shared_ptr<MqttManager> mqtt_manager, int port = 8080);
  ~HttpServer();

  void Start();
  void Stop();
  bool IsRunning() const { return running_; }
};

#endif  // HTTP_SERVER_H_
