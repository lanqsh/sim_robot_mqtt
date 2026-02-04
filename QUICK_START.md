# 快速开始指南

## 1. 环境准备

### 安装所有依赖（Ubuntu/Debian）

```bash
# 系统基础工具
sudo apt update
sudo apt install -y build-essential cmake git

# MQTT库
sudo apt install -y libpaho-mqtt-dev libpaho-mqttpp-dev

# 数据库和JSON
sudo apt install -y libsqlite3-dev nlohmann-json3-dev

# 日志和HTTP服务器
sudo apt install -y libgoogle-glog-dev libhttplib-dev
```

### 或使用vcpkg（推荐用于Windows）

```bash
vcpkg install paho-mqttpp3 sqlite3 nlohmann-json glog cpp-httplib
```

## 2. 下载cpp-httplib（如果包管理器没有）

```bash
cd include/
wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
cd ..
```

## 3. 准备前端文件

前端文件已组织在 `web/` 文件夹中：

```
web/
├── index.html    # HTML结构
├── style.css     # CSS样式
└── app.js        # JavaScript逻辑
```

**重要**: 运行程序时，确保 `web/` 文件夹在可执行文件的同一目录！

## 4. 编译项目

```bash
# 创建并进入构建目录
mkdir -p build
cd build

# 配置项目
cmake ..

# 编译（使用多核加速）
make -j$(nproc)

# 返回项目根目录
cd ..
```

## 5. 运行程序

```bash
# 方法1: 在项目根目录运行（推荐，web文件夹已在正确位置）
./build/robot

# 方法2: 在build目录运行（需要复制web文件夹）
cp -r web build/
cd build
./robot
```

## 6. 访问Web界面

程序启动后，日志中会显示：

```
HTTP服务器地址: http://localhost:8080
```

在浏览器中打开: **http://localhost:8080**

## 7. 测试功能

### 7.1 Web界面测试

1. **查看机器人列表**
   - 页面会自动显示数据库中启用的机器人

2. **添加新机器人**
   - 在表单中输入机器人ID（必填）
   - 输入机器人名称（可选）
   - 点击"添加机器人"按钮
   - 成功后会显示绿色提示消息

3. **查看机器人数据**
   - 点击任意机器人卡片
   - 弹出模态框显示详细数据
   - 包含电池、电机、光伏等实时信息

4. **删除机器人**
   - 点击机器人卡片上的"删除"按钮
   - 确认删除操作
   - 机器人会从列表中移除

### 7.2 API测试（使用curl）

```bash
# 1. 获取所有机器人
curl http://localhost:8080/api/robots

# 2. 添加新机器人
curl -X POST http://localhost:8080/api/robots \
  -H "Content-Type: application/json" \
  -d '{
    "robot_id": "test_robot_001",
    "robot_name": "测试机器人"
  }'

# 3. 查看机器人详细数据
curl http://localhost:8080/api/robots/test_robot_001/data

# 4. 删除机器人
curl -X DELETE http://localhost:8080/api/robots/test_robot_001
```

### 7.3 观察日志输出

程序会输出详细的日志信息：

```
I... Robot MQTT Simulator v1.0.0
I... === 配置信息 ===
I... Broker: tcp://lanq.top:10043
I... Client ID: sim_robot_cpp
I... QoS: 1
I... HTTP Port: 8080
I... 启用的机器人 (2):
I...   - 303930306350729d
I...   - 303930306350729e
I... ==================
I... MQTT客户端已连接
I... HTTP服务器启动在端口: 8080
I... 程序运行中，按 Ctrl+C 退出...
I... HTTP服务器地址: http://localhost:8080
```

## 8. 动态测试功能

程序包含一个自动测试线程，会在启动后：

- 30秒后自动添加一个测试机器人（ID: `303930306350729g`）
- 再60秒后（共90秒）删除该测试机器人

观察日志可以看到完整的添加和删除过程。

## 9. 常见问题

### 问题1: 编译时找不到httplib.h

**解决方法**:
```bash
# 下载到include目录
cd include/
wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

### 问题2: 端口8080被占用

**解决方法**:
```bash
# 修改数据库配置
sqlite3 config.db
UPDATE mqtt_config SET value = '9090' WHERE key = 'http_port';
.quit

# 或删除数据库让程序重新创建
rm config.db
```

### 问题3: 浏览器显示404 Not Found

**原因**: web文件夹不在可执行文件同一目录

**解决方法**:
```bash
# 确保web文件夹在运行目录
ls -l web/

# 如果不在web文件夹，复制到build目录
cp -r web build/
cd build
./robot

# 或者在项目根目录运行
./build/robot
```

### 问题4: MQTT连接失败

**检查**:
- MQTT Broker地址是否正确
- 网络连接是否正常
- 防火墙是否阻止连接

**修改Broker地址**:
```bash
sqlite3 config.db
UPDATE mqtt_config SET value = 'tcp://test.mosquitto.org:1883' WHERE key = 'broker';
.quit
```

### 问题5: 编译时链接错误

**可能原因**: 依赖库未正确安装

**检查依赖**:
```bash
# 检查库文件是否存在
ldconfig -p | grep paho
ldconfig -p | grep sqlite3
ldconfig -p | grep glog

# 重新安装依赖
sudo apt install --reinstall libpaho-mqttpp-dev libsqlite3-dev libgoogle-glog-dev
```

## 10. 验证成功运行的标志

✅ **编译成功**:
```
[100%] Built target robot
```

✅ **程序启动成功**:
```
I... MQTT客户端已连接
I... HTTP服务器线程启动，监听端口: 8080
I... 程序运行中，按 Ctrl+C 退出...
```

✅ **Web界面可访问**:
- 浏览器能打开 http://localhost:8080
- 显示机器人管理系统界面
- 能看到机器人列表

✅ **MQTT工作正常**:
```
I... [Robot 303930306350729d] 上报数据已加入队列
I... 消息已发送到主题: application/.../device/303930306350729d/event/up
```

✅ **HTTP API工作正常**:
```
I... API: 获取机器人列表, 数量: 2
I... API: 添加机器人成功 - test_robot_001 (测试机器人)
```

## 11. 下一步

现在您可以：

1. 通过Web界面管理机器人
2. 使用REST API集成到您的系统
3. 查看日志目录`./logs`中的详细日志
4. 修改数据库配置自定义行为
5. 开发自己的功能扩展

## 技术支持

如遇到其他问题，请查看：
- `README.md` - 完整项目文档
- `HTTP_IMPLEMENTATION.md` - HTTP功能实现细节
- `HTTP_SERVER_SETUP.md` - HTTP服务器安装指南
- 日志文件 `./logs/` - 详细的运行日志
