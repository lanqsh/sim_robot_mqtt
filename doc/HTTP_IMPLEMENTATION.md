# HTTP服务器功能实现总结

> 最后更新：2026-03-13，v1.3.0

## 已完成的工作

### 1. 数据库结构 ✅

**修改文件**: `src/config_db.cpp`, `include/config_db.h`

- `robots` 表字段：robot_id, serial_number, robot_name, enabled, alarm_fa/fb/fc/fd, data_snapshot (JSON快照)
- `mqtt_config` 表包含 http_port 等配置项
- `ConfigDb::AddRobot()` — 支持 name/id/serial_number 三选一 + enabled
- `ConfigDb::UpdateRobotInfo()` — 更新名称/ID/启用状态，ID变更时自动同步MqttManager
- `ConfigDb::UpdateRobotDataSnapshot()` — 保存最近一次上报数据快照
- `ConfigDb::UpdateRobotAlarms()` / `GetRobotAlarms()`

### 2. HttpServer类实现 ✅

**新增文件**:
- `include/http_server.h` - HTTP服务器类声明
- `src/http_server.cpp` - HTTP服务器实现

**功能特性**:
- 使用 cpp-httplib 库实现轻量级HTTP服务器
- 支持CORS跨域请求
- 在独立线程中运行，不阻塞主程序
- 提供静态文件服务（index.html）

### 3. REST API端点实现 ✅

**实现的API**:

1. **GET /api/robots** - 获取所有机器人列表
   - 返回JSON数组，包含robot_id, robot_name, enabled

2. **POST /api/robots** - 添加新机器人
   - 请求体: `{"robot_id": "...", "robot_name": "..."}`
   - 自动添加到数据库和MqttManager
   - 返回操作结果

3. **DELETE /api/robots/{id}** - 删除机器人
   - 从MqttManager和数据库中移除
   - 自动取消MQTT订阅

4. **GET /api/robots/{id}/data** - 获取机器人详细数据
   - 返回机器人实时状态和数据
   - 包含电池、电机、光伏等信息

5. **GET /api/robots/{id}/alarms** - 获取机器人告警配置
   - 支持按robot_id或serial查询（通过查询2参数type=id|serial）
   - 直接从内存中的Robot对象获取告警数据
   - 返回4类告警的配置值：alarm_fa, alarm_fb, alarm_fc, alarm_fd

6. **PATCH /api/robots/{id}/alarms** - 设置机器人告警
   - 支持按robot_id或serial更新（通过查询2参数type=id|serial）
   - 请求体: `{"alarm_fa": 134217727, "alarm_fb": 2047, "alarm_fc": 2147483647, "alarm_fd": 31}`
   - 同时更新内存中的Robot对象和数据库
   - 调用Robot::UpdateAlarmsToDb()统一接口进行数据库持久化

### 4. 前端HTML页面 ✅

**新增文件**:
- `web/index.html` - HTML结构
- `web/style.css` - CSS样式
- `web/app.js` - JavaScript逻辑

**界面功能**:
- 📝 **添加机器人表单**: 输入ID和名称，提交添加
- 📊 **机器人卡片展示**: 网格布局显示所有机器人
- 🟢/🔴 **状态指示器**: 显示机器人启用/禁用状态
- 🗑️ **删除按钮**: 快速删除机器人
- 📈 **查看数据**: 点击卡片查看详细实时数据
- 🔄 **自动刷新**: 每10秒自动更新列表
- 🎨 **现代UI设计**: 渐变背景、卡片悬停效果、响应式布局

**文件结构**:
- **index.html** (52行): 简洁的HTML结构，引用外部CSS和JS
- **style.css** (268行): 完整的样式定义，包括响应式设计
- **app.js** (234行): 所有JavaScript功能，包括加载、添加、删除和查看数据

### 5. MqttManager增强 ✅

**修改文件**: `src/mqtt_manager.cpp`, `include/mqtt_manager.h`

- `AddRobot(robot_id)` / `RemoveRobot(robot_id)` / `GetRobot(robot_id)`
- robot_id 变更时支持自动重建订阅

### 6. Robot类增强 ✅

**修改文件**: `src/robot.cpp`, `include/robot.h`

- `IsRunning()`, `GetLastData()`, `GetData()`, `SerializeDataSnapshot()`
- `software_version` 字段，`fault_status` 实时计算
- `MotorSimConfig`, `AlarmSimConfig` 模拟配置结构

### 7. 主程序集成 ✅

**修改文件**: `src/main.cpp`

- 创建并启动 HttpServer 实例
- 从数据库读取 `http_port` 配置
- 已移除 TestAddRemoveRobot 测试函数

### 8. 构建配置更新 ✅

**修改文件**: `CMakeLists.txt`

- 添加 `src/http_server.cpp` 到编译列表

### 9. 告警管理功能 ✅

- 数据库字段：alarm_fa/fb/fc/fd（INTEGER位掩码）
- 内存优先：获取告警直接从 Robot 对象读取
- `Robot::UpdateAlarmsToDb()` 统一持久化接口
- API：GET `/api/v1/robots/get_alarms` / POST `/api/v1/robots/set_alarms`

### 10. 数据模拟功能 ✅

- 每个机器人维护 `MotorSimConfig`（电机电流模拟）和 `AlarmSimConfig`（随机告警模拟）
- 配置通过 `config_db_` 持久化到数据库快照
- 告警模拟 fc_bits_mask 以 fc_bit_0~fc_bit_31 独立字段传输，后端组装成 uint32 掩码
- API：GET/POST `/api/v1/robots/sim_config/motor` 和 `/api/v1/robots/sim_config/alarm`

### 11. 文档更新 ✅

各模块详细说明见 ARCHITECTURE.txt / HTTP_SERVER_SETUP.md / OPENAPI.yaml。

## 技术要点

### 线程安全
- 使用 `std::mutex` 保护共享数据（robots_map）
- HTTP服务器运行在独立线程
- 智能指针管理对象生命周期

### 智能指针使用
- `std::shared_ptr<ConfigDb>` - 配置数据库
- `std::shared_ptr<MqttManager>` - MQTT管理器
- `std::weak_ptr<MqttManager>` - Robot中避免循环引用

### JSON数据格式
- 使用 nlohmann/json 库处理JSON
- API响应统一格式：`{"success": true/false, "error": "..."}`

### 错误处理
- 所有API端点都有try-catch错误处理
- 返回适当的HTTP状态码（200, 400, 404, 500）
- 在日志中记录所有操作和错误

## 使用方法

### 1. 安装依赖

```bash
# Ubuntu/Debian
sudo apt install libhttplib-dev

# 或使用vcpkg
vcpkg install cpp-httplib
```

### 2. 编译项目

```bash
mkdir -p build && cd build
cmake ..
make
```

### 3. 运行程序

```bash
./robot
```

### 4. 访问Web界面

打开浏览器访问: `http://localhost:8080`

## 配置说明

在数据库中可配置HTTP端口：

```sql
-- 修改HTTP端口
UPDATE mqtt_config SET value = '9090' WHERE key = 'http_port';
```

## API测试示例

### 使用curl测试

```bash
# 获取机器人列表
curl http://localhost:8080/api/robots

# 添加机器人
curl -X POST http://localhost:8080/api/robots \
  -H "Content-Type: application/json" \
  -d '{"robot_id":"test123","robot_name":"Test Robot"}'

# 删除机器人
curl -X DELETE http://localhost:8080/api/robots/test123

# 获取机器人数据
curl http://localhost:8080/api/robots/303930306350729d/data
```

## 注意事项

1. **cpp-httplib依赖**: 确保已正确安装cpp-httplib库
2. **端口占用**: 如果8080端口被占用，修改数据库中的http_port配置
3. **web文件夹位置**: web文件夹必须与可执行文件在同一目录
4. **跨域支持**: 已配置CORS，支持跨域请求
5. **自动刷新**: 前端页面每10秒自动刷新机器人列表
6. **文件分离**: 前端代码分为HTML、CSS、JS三个独立文件，便于维护

## 后续优化建议

- [ ] 添加WebSocket支持实现实时数据推送
- [ ] 添加用户认证和权限管理
- [ ] 支持批量操作（批量启用/禁用）
- [ ] 添加数据图表展示（使用Chart.js等）
- [ ] 支持导出机器人数据（CSV/Excel）
- [ ] 添加操作日志记录
- [ ] 支持机器人分组管理
