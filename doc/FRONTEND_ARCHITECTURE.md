# 前端模块化结构说明

## 文件结构

```
web/
├── html/
│   └── index.html     # HTML 主页面
├── css/               # 样式文件目录
│   ├── variables.css  # CSS 变量（色彩、间距等）
│   ├── base.css       # 基础样式（reset、通用元素）
│   ├── layout.css     # 布局样式（header、容器）
│   ├── components.css # 组件样式（按钮、卡片、模态框）
│   ├── tables.css     # 表格样式
│   ├── pages.css      # 页面特定样式
│   └── utils.css      # 工具类（辅助样式）
└── js/                # JavaScript 模块目录
    ├── app.js         # 主入口文件（协调各模块）
    ├── config.js      # 配置常量（API地址、字段映射等）
    ├── api.js         # API 调用封装
    ├── ui.js          # UI 渲染和更新
    ├── pagination.js  # 分页管理
    ├── robot-operations.js  # 机器人操作（增删改查）
    └── commands.js    # 命令发送（定时启动、启动、校时、告警）
```

## 模块职责

### 1. config.js - 配置模块
- API 基础地址
- 字段中文映射表
- 星期名称映射
- 其他全局常量

### 2. api.js - API 调用模块
封装所有后端 API 调用：
- `fetchRobots(filters)` - 获取机器人列表（支持 robot_id/robot_name/enabled 过滤参数）
- `addRobot(robotData)` - 添加机器人
- `updateRobot(robotData)` - 编辑机器人信息
- `deleteRobot(robotId)` - 删除机器人
- `fetchRobotData(robotId)` - 获取实时数据
- `batchAddRobots(robots)` - 批量添加
- `batchDeleteRobots(robotIds)` - 批量删除
- `sendScheduleStartRequest()` - 定时启动请求
- `sendStartRequest()` - 立即启动请求
- `sendTimeSyncRequest()` - 校时请求
- `getRobotAlarms(robotId)` - 获取机器人告警配置
- `setRobotAlarms(robotId, alarmData)` - 设置机器人告警
- `getSimConfigMotor(robotId)` - 获取电机电流模拟配置
- `setSimConfigMotor(robotId, config)` - 设置电机电流模拟配置
- `getSimConfigAlarm(robotId)` - 获取随机告警模拟配置
- `setSimConfigAlarm(robotId, config)` - 设置随机告警模拟配置

### 3. ui.js - UI 渲染模块
负责所有UI更新和渲染：
- `updateStatistics(statistics)` - 更新统计信息
- `renderRobots(robots)` - 渲染机器人列表
- `renderRobotData(data)` - 渲染详细数据
- `renderAlarmSettings(alarmData)` - 渲染告警设置界面
- `showEmptyState()` - 显示空状态
- `showError(message)` - 显示错误
- `showLoading(text)` / `hideLoading()` - 加载提示
- `closeModal()` - 关闭模态框
- `openAlarmModal()` / `closeAlarmModal()` - 打开/关闭告警模态框
- `toggleForm(contentId, iconId)` - 切换表单显示

### 4. pagination.js - 分页管理模块
分页逻辑封装在 `PaginationManager` 类中：
- `updatePagination(pagination)` - 更新分页信息
- `renderPagination()` - 渲染分页控件
- `goToPage(page)` - 跳转页码
- `changePageSize(newPageSize)` - 改变页面大小
- `getCurrentPage()` / `getPageSize()` - 获取当前状态

### 5. robot-operations.js - 机器人操作模块
机器人管理相关操作：
- `addRobot()` - 添加单个机器人
- `deleteRobot()` - 删除机器人
- `openEditModal(robotId)` - 打开编辑机器人模态框
- `closeEditModal()` - 关闭编辑模态框
- `saveEditRobot()` - 保存编辑结果（调用 updateRobot API）
- `viewRobotData()` - 查看详细数据
- `batchAdd()` - 批量添加
- `batchDelete()` - 批量删除

### 6. commands.js - 命令发送模块
机器人命令发送和告警管理：
- `sendScheduleRequest()` - 发送定时启动请求
- `sendStartRequest()` - 发送启动请求
- `sendTimeSyncRequest()` - 发送校时请求
- `loadAlarmData()` - 加载机器人告警数据并更新复选框
- `switchAlarmTab(type)` - 切换告警标签页（FA/FB/FC/FD）

### 7. app.js - 主入口模块
- 导入所有模块
- 初始化应用
- 暴露全局函数供 HTML 调用
- 设置事件监听器
- 定时刷新任务

**全局函数：**
- `window.searchRobots()` - 按条件搜索机器人
- `window.clearSearch()` - 清除搜索条件
- `window.openAlarmSettings(robotId)` - 打开告警设置模态框
- `window.saveAlarmSettings()` - 保存告警配置
- `window.closeAlarmModal()` - 关闭告警模态框
- `window.switchAlarmTab(type)` - 切换告警标签页（FA/FB/FC/FD）
- `window.openEditModal(robotId)` - 打开编辑机器人模态框
- `window.closeEditModal()` - 关闭编辑模态框
- `window.saveEditRobot()` - 保存编辑结果
- `window.openSimConfigModal(robotId)` - 打开模拟配置模态框
- `window.closeSimConfigModal()` - 关闭模拟配置模态框
- `window.saveSimConfig()` - 保存模拟配置
- `window.switchSimTab(tab)` - 切换模拟标签页（motor/alarm）
- 其他机器人操作和命令发送函数

## 模块依赖关系

```
app.js (主入口)
├── api.js (API调用)
│   └── config.js (配置)
├── ui.js (UI渲染)
│   └── config.js (配置)
├── pagination.js (分页管理)
├── robot-operations.js (机器人操作)
│   ├── api.js
│   └── ui.js
└── commands.js (命令发送)
    ├── api.js
    ├── ui.js
    └── config.js
```

## 使用 ES6 模块

所有 JavaScript 文件使用 ES6 模块语法：
- 使用 `export` 导出函数、类、常量
- 使用 `import` 导入依赖
- HTML 中使用 `<script type="module">` 引入

## 优势

1. **模块化清晰**：每个文件职责单一，易于维护
2. **代码复用**：API、UI 等模块可独立复用
3. **易于测试**：模块间解耦，便于单元测试
4. **便于扩展**：添加新功能只需修改对应模块
5. **减少冲突**：模块作用域隔离，避免全局变量污染

## 注意事项

- 使用 ES6 模块需要通过 HTTP 服务器访问，不能直接打开 HTML 文件
- 全局函数通过 `window.xxx = function()` 暴露给 HTML 内联事件调用
- 模块间通过 import/export 进行通信，避免全局变量
