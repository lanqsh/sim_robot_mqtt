# HTTP服务器功能实现总结

## 已完成的工作

### 1. 数据库结构更新 ✅

**修改文件**: `src/config_db.cpp`, `include/config_db.h`

- 在 `robots` 表中添加了 `robot_name TEXT` 字段用于存储机器人名称
- 在 `mqtt_config` 表中添加了 `http_port` 配置项，默认值为 8080
- 更新了 `AddRobot()` 方法签名，增加 `robot_name` 参数
- 新增了 `GetAllRobots()` 方法，返回包含所有机器人信息的列表
- 定义了 `RobotInfo` 结构体存储机器人信息（id, name, enabled）

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

- 新增 `AddRobot(const std::string& robot_id)` 重载方法
  - 通过robot_id直接添加机器人
  - 自动从配置读取主题和间隔
  - 创建Robot实例并启动上报线程

- 新增 `GetRobot(const std::string& robot_id)` 方法
  - 返回指定机器人的shared_ptr
  - 用于HTTP API获取机器人数据

### 6. Robot类增强 ✅

**修改文件**: `src/robot.cpp`, `include/robot.h`

- 新增 `IsRunning()` 方法：返回机器人运行状态
- 新增 `GetLastData()` 方法：返回JSON格式的机器人数据
  - 包含电池电量、电压、电流、温度
  - 主从电机电流
  - 工作时长、运行次数、圈数
  - 光伏电压、电流
  - 主板温度

### 7. 主程序集成 ✅

**修改文件**: `src/main.cpp`

- 添加 `#include "http_server.h"`
- 从数据库读取 `http_port` 配置
- 创建并启动 HttpServer 实例
- 在日志中输出HTTP服务器访问地址
- 更新测试函数使用新的API

### 8. 构建配置更新 ✅

**修改文件**: `CMakeLists.txt`

- 添加 `src/http_server.cpp` 到编译列表

### 9. 告警管理功能 ✅

**修改文件**: `src/config_db.cpp`, `include/config_db.h`, `src/robot.cpp`, `include/robot.h`, `src/http_server.cpp`

#### 9.1 数据库结构扩展
- 在 `robots` 表中添加四个告警字段：
  - `alarm_fa INTEGER DEFAULT 0` - FA类告警（27位）
  - `alarm_fb INTEGER DEFAULT 0` - FB类告警（11位）
  - `alarm_fc INTEGER DEFAULT 0` - FC类告警（31位）
  - `alarm_fd INTEGER DEFAULT 0` - FD类告警（5位）
- 定义 `AlarmData` 结构体存储告警配置

#### 9.2 ConfigDb增强
- 新增 `UpdateRobotAlarms(const std::string& robot_id, const AlarmData& alarm)` 方法
  - 更新指定机器人的告警配置到数据库
- 新增 `GetRobotAlarms(const std::string& robot_id)` 方法
  - 从数据库读取指定机器人的告警配置

#### 9.3 Robot类增强
- 添加 `weak_ptr<ConfigDb> config_db_` 成员变量
- 新增 `SetConfigDb(std::shared_ptr<ConfigDb> config_db)` 方法
  - 设置数据库引用，避免循环引用
- 新增 `UpdateAlarmsToDb()` 方法
  - 统一的告警持久化接口
  - 将当前内存中的告警数据更新到数据库

#### 9.4 MqttManager增强
- 将 `ConfigDb&` 改为 `std::shared_ptr<ConfigDb>`
- 在 `AddRobot()` 时调用 `robot->SetConfigDb(config_db_)`
- 程序启动时自动从数据库加载每个机器人的告警配置

#### 9.5 HTTP API端点
**GET /api/robots/{identifier}/alarms?type=id|serial**
- 从内存中的Robot对象获取告警数据（不读数据库）
- 支持按robot_id或serial查询
- 返回格式：`{"success": true, "robot_id": "...", "alarm_fa": 0, "alarm_fb": 0, "alarm_fc": 0, "alarm_fd": 0}`

**PATCH /api/robots/{identifier}/alarms?type=id|serial**
- 更新机器人的告警配置
- 同时更新内存中的Robot对象和数据库
- 调用 `robot->UpdateAlarmsToDb()` 进行数据库持久化
- 请求体：`{"alarm_fa": 134217727, "alarm_fb": 2047, "alarm_fc": 2147483647, "alarm_fd": 31}`

#### 9.6 前端界面
- 每个机器人卡片添加“告警设置”按钮
- 点击按钮打开模态框，显示4个标签页（FA/FB/FC/FD）
- 每个标签页展示对应类型的所有告警项（两列布局）
- 支持勾选/取消勾选告警项，点击保存后调用PATCH API
- 打开模态框时自动从后端加载告警配置
- 切换标签页时自动加载对应类型的告警数据

#### 9.7 技术要点
- **内存优先**: 获取告警时直接从robot对象读取，避免每次都查询数据库
- **统一持久化**: Robot类提供UpdateAlarmsToDb()统一接口，告警变化时调用
- **智能指针管理**: Robot使用weak_ptr引用ConfigDb，避免循环引用
- **位掩码存储**: 每个告警类型使用一个整数，每个位表示一个告警项
- **模态框设计**: 告警设置采用模态框，不占用页面固定区域

### 10. 文档完善 ✅

**更新文件**:
- `README.md` - 添加Web管理界面和REST API说明
- `HTTP_SERVER_SETUP.md` - cpp-httplib安装指南

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
