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
- 前端文件位置: web/ 文件夹（html/index.html, css/, js/）
- API端点:
  - GET /api/robots - 获取所有机器人列表
  - POST /api/robots - 添加机器人
  - DELETE /api/robots/{id} - 删除机器人
  - GET /api/robots/{id}/data - 获取机器人详细数据
  - GET /api/robots/{id}/alarms?type=id|serial - 获取机器人告警配置
  - PATCH /api/robots/{id}/alarms?type=id|serial - 设置机器人告警
