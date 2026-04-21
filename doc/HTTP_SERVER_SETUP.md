# HTTP Server 依赖说明

本项目使用 cpp-httplib 库来实现HTTP服务器功能。

## 安装方法

### 方法1: 使用vcpkg (推荐)
```bash
vcpkg install cpp-httplib
```

### 方法2: 手动安装
1. 从 https://github.com/yhirose/cpp-httplib 下载 httplib.h
2. 将 httplib.h 放到 include/ 目录下

### 方法3: 使用系统包管理器

#### Ubuntu/Debian:
```bash
sudo apt-get install libhttplib-dev
```

#### macOS (Homebrew):
```bash
brew install cpp-httplib
```

## CMakeLists.txt 配置

如果使用vcpkg，需要在CMakeLists.txt中添加：
```cmake
find_package(httplib CONFIG REQUIRED)
target_link_libraries(robot PRIVATE httplib::httplib)
```

如果手动安装httplib.h到include目录，则不需要额外配置。

## HTTP服务器功能

- 端口: 默认8080 (可在数据库中配置)
- 前端页面: http://localhost:8080
- 前端文件位置: `web/` 文件夹（`html/index.html`, `css/`, `js/`）
- 完整 API 规范见 `doc/OPENAPI.yaml`

### 主要 API 端点（v1.3.0）

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/v1/robots/get` | 获取机器人列表（支持过滤+分页） |
| POST | `/api/v1/robots/add` | 添加机器人 |
| POST | `/api/v1/robots/update` | 编辑机器人信息 |
| POST | `/api/v1/robots/delete` | 删除机器人 |
| POST | `/api/v1/robots/batch_add` | 批量添加 |
| POST | `/api/v1/robots/batch_delete` | 批量删除 |
| GET/POST | `/api/v1/robots/data` | 实时数据查询/快照推送 |
| GET/POST | `/api/v1/robots/runtime_data` | 运行时覆盖数据 |
| GET | `/api/v1/robots/get_alarms` | 获取告警配置 |
| POST | `/api/v1/robots/set_alarms` | 设置告警配置 |
| GET/POST | `/api/v1/robots/report/e0`~`e8` | E帧上报参数 |
| GET/POST | `/api/v1/robots/motor_params` | 电机参数 |
| GET/POST | `/api/v1/robots/battery_params` | 电池参数 |
| GET/POST | `/api/v1/robots/schedule_params` | 调度参数 |
| GET/POST | `/api/v1/robots/parking_position` | 停靠位参数 |
| GET/POST | `/api/v1/robots/lora_params` | LoRa 参数 |
| GET/POST | `/api/v1/robots/daytime_scan_protect` | 防误扫参数 |
| GET | `/api/v1/robots/mqtt_messages` | MQTT 消息记录 |
| GET | `/api/v1/robots/request_reply` | 请求/回复记录 |
| POST | `/api/v1/robots/schedule_start` | 定时启动 |
| POST | `/api/v1/robots/start` | 立即启动 |
| POST | `/api/v1/robots/time_sync` | 校时 |
| POST | `/api/v1/robots/control` | B0~BA 控制帧 |
| POST | `/api/v1/robots/trigger_report` | 触发即时上报 |
| POST | `/api/v1/robots/lora_clean_settings` | LoRa 清扫设置 |
| POST | `/api/v1/robots/robot_data` | 机器人数据上报 |
| GET/POST | `/api/v1/robots/sim_config/motor` | 电机电流模拟配置 |
| GET/POST | `/api/v1/robots/sim_config/alarm` | 随机告警模拟配置 |
| GET | `/api/v1/system/version` | 系统版本 |
| GET/POST | `/api/v1/system/report_intervals` | 上报间隔 |
| GET/POST | `/api/v1/system/mqtt_config` | MQTT 配置 |
| GET | `/api/v1/system/firmware` | 固件信息 |
| POST | `/api/v1/system/robot_version` | 更新机器人软件版本 |
| GET | `/api/v1/system/firmware/download` | 固件下载 |
