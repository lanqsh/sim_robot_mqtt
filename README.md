# sim_robot_mqtt

一个使用 MQTT 协议的机器人模拟器，基于 C++ 和 Eclipse Paho MQTT 库实现。

## 项目简介

该项目模拟一个机器人客户端，通过 MQTT 协议：
- 支持多个发布主题，发送遥测数据（包含序列号、电池电量和机器人 ID）
- 支持多个订阅主题，接收控制命令
- 使用 SQLite 数据库存储配置，支持动态机器人 ID 替换

## 依赖项

- CMake 3.14 或更高版本
- C++17 兼容的编译器（GCC 7+, Clang 5+, MSVC 2017+）
- Eclipse Paho MQTT C 库
- Eclipse Paho MQTT C++ 库
- SQLite3 库（用于配置存储）

### 安装依赖库

```bash
# 更新软件包列表
sudo apt update

# 安装编译工具
sudo apt install build-essential cmake git

# 安装 Paho MQTT C 库
sudo apt install libpaho-mqtt-dev

# 安装 Paho MQTT C++ 库
sudo apt install libpaho-mqttpp-dev

# 安装 SQLite3 库
sudo apt install libsqlite3-dev

# 安装 glog 日志库（从源码编译）
sudo apt install libgoogle-glog-dev
```

## 编译方法

```bash
# 创建构建目录
mkdir -p build
cd build

# 配置项目
cmake ..

# 编译
make
```

## 数据库配置

项目使用 SQLite 数据库 (`config.db`) 存储配置信息。首次运行时会自动创建数据库并初始化默认配置。

### 数据库结构

数据库包含两个表：

#### 1. mqtt_config 表
存储 MQTT 连接配置和主题模板：

| key | value | 说明 |
|-----|-------|------|
| broker | tcp://lanq.top:10043 | MQTT broker 地址 |
| client_id_prefix | sim_robot_cpp | 客户端 ID 前缀 |
| qos | 1 | 消息质量等级 (0/1/2) |
| keepalive | 60 | 心跳间隔（秒） |
| publish_interval | 1 | 发布消息间隔（秒） |
| default_duration | 30 | 默认运行时长（秒） |
| publish_topic | application/.../device/{robot_id}/command/up | 发布主题模板 |
| subscribe_topic | application/.../device/{robot_id}/command/down | 订阅主题模板 |

**说明**：
- 主题模板中的 `{robot_id}` 会在运行时自动替换为实际的机器人 ID
- 每个机器人只有一个发布主题和一个订阅主题
- 发布主题用于向服务器发送数据（command/up）
- 订阅主题用于接收服务器指令（command/down）

#### 2. robots 表
存储机器人列表：

| id | robot_id | enabled | 说明 |
|----|----------|---------|------|
| 1 | 303930306350729d | 1 | 启用 |
| 2 | 303930306350729e | 0 | 禁用 |
| 3 | 303930306350729f | 0 | 禁用 |

**说明**：
- `robot_id`：机器人的唯一标识符
- `enabled`：1 表示启用，0 表示禁用
- 程序会自动使用第一个启用的机器人
- 支持多个机器人，但每次只运行一个

### 修改配置

可以使用任何 SQLite 客户端工具修改配置：

```bash
# 使用 sqlite3 命令行工具
sqlite3 config.db

# 查看所有配置
SELECT * FROM mqtt_config;

# 修改 broker 地址
UPDATE mqtt_config SET value = 'tcp://localhost:1883' WHERE key = 'broker';

# 修改发布主题模板
UPDATE mqtt_config SET value = 'application/your-app-id/device/{robot_id}/command/up' WHERE key = 'publish_topic';

# 修改订阅主题模板
UPDATE mqtt_config SET value = 'application/your-app-id/device/{robot_id}/command/down' WHERE key = 'subscribe_topic';

# 查看所有机器人
SELECT * FROM robots;

# 添加新机器人
INSERT INTO robots (robot_id, enabled) VALUES ('robot_new_001', 1);

# 启用/禁用机器人
UPDATE robots SET enabled = 1 WHERE robot_id = '303930306350729e';
UPDATE robots SET enabled = 0 WHERE robot_id = '303930306350729d';

# 删除机器人
DELETE FROM robots WHERE robot_id = 'robot_new_001';

# 退出
.quit
```

### 设计优势

**1. 主题模板统一管理**
- 所有机器人共享相同的主题结构
- 只需维护一套主题模板
- 减少数据冗余

**2. 灵活的机器人管理**
- 支持多个机器人
- 通过 `enabled` 字段轻松切换
- 添加新机器人无需修改主题配置

**3. 动态主题生成**
- 运行时自动将 `{robot_id}` 替换为实际 ID
- 避免硬编码
- 易于扩展到多机器人场景

## 运行方法

编译成功后，可执行文件 `robot` 会生成在 `build` 目录下。

**注意**：程序会在当前目录下创建 `logs` 目录用于存储日志文件。日志同时输出到终端和文件系统。

### 基本用法

```bash
# 使用默认配置（自动使用 config.db）
./robot

# 指定运行时长（秒）
./robot 60

# 指定 broker 地址
./robot tcp://localhost:1883

# 同时指定 broker 和运行时长
./robot tcp://localhost:1883 120
```

### 参数说明

```
./robot [broker_url] [duration]
```

- `broker_url`：MQTT broker 地址（可选，覆盖数据库配置）
- `duration`：运行时长，单位为秒（可选，覆盖数据库配置）

### 运行示例

```bash
# 使用数据库配置运行 60 秒
./robot 60

# 连接到本地 broker
./robot tcp://localhost:1883 60

# 连接到自定义 broker
./robot tcp://192.168.1.100:1883 120
```

### 程序输出示例

```
=== 配置信息 ===
Broker: tcp://lanq.top:10043
Robot ID: 303930306350729d
Client ID: sim_robot_cpp_303930306350729d
QoS: 1
Duration: 30 seconds

启用的机器人 (1):
  - 303930306350729d (当前使用)

发布主题: application/.../device/303930306350729d/command/up
订阅主题: application/.../device/303930306350729d/command/down
==================

正在连接到 broker: tcp://lanq.top:10043
连接成功!

正在订阅主题: application/.../device/303930306350729d/command/down
订阅完成! 等待消息...

开始发布消息 (运行 30 秒)...
[1/30] 已发布: {"seq": 0, "battery": 100, "robot_id": "303930306350729d"}
[2/30] 已发布: {"seq": 1, "battery": 99, "robot_id": "303930306350729d"}
...
```

## 测试 MQTT 通信

你可以使用 `mosquitto_sub` 和 `mosquitto_pub` 工具来测试：

### 订阅遥测数据

```bash
mosquitto_sub -h test.mosquitto.org -t "sim/robot/telemetry"
```

### 发送控制命令

```bash
mosquitto_pub -h test.mosquitto.org -t "sim/robot/control" -m "stop"
```

## 项目结构

```
sim_robot_mqtt/
├── CMakeLists.txt      # CMake 构建配置
├── README.md           # 项目说明文档
├── config.db           # SQLite 配置数据库（首次运行自动创建）
└── src/
    └── main.cpp        # 主程序源代码
```

## 故障排除

### 找不到 SQLite3 库

如果 CMake 提示找不到 SQLite3：

```bash
sudo apt install libsqlite3-dev
```

### 找不到 Paho MQTT 库

如果 CMake 提示找不到 PahoMqttCpp：

```bash
sudo apt install libpaho-mqtt-dev libpaho-mqttpp-dev

# 检查库是否正确安装
dpkg -l | grep paho
```

### 数据库权限问题

如果程序无法创建或访问 config.db：
```bash
# 检查当前目录权限
ls -la config.db

# 删除并重新创建数据库
rm config.db
./robot
```

### 编译时链接错误

确保安装了所有必需的库：
- `libpaho-mqtt-dev`（Paho MQTT C 库）
- `libpaho-mqttpp-dev`（Paho MQTT C++ 库）
- `libsqlite3-dev`（SQLite3 库）

### 运行时连接问题

如果无法连接到 MQTT broker：
1. 检查 broker 地址是否正确
2. 确认 broker 服务是否运行
3. 检查防火墙设置
4. 尝试使用公共测试 broker：`tcp://test.mosquitto.org:1883`

## 许可证

本项目为示例代码，可自由使用和修改。

## 参考资源

- [Eclipse Paho MQTT 官网](https://www.eclipse.org/paho/)
- [MQTT 协议文档](https://mqtt.org/)
- [Mosquitto MQTT Broker](https://mosquitto.org/)
- [SQLite 官网](https://www.sqlite.org/)
