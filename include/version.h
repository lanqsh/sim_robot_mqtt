#ifndef VERSION_H_
#define VERSION_H_

// ============================================================
// 后端版本号 —— 每次发版或 API 变更时手动更新此处
// 格式：MAJOR.MINOR.PATCH
//   MAJOR：重大不兼容变更
//   MINOR：新增接口或功能（向后兼容）
//   PATCH：Bug 修复或修正
// ============================================================
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 4
#define APP_VERSION_PATCH 0

#define APP_VERSION_STR "1.4.0"

// 版本历史
// 1.3.0 (2026-03-03) - 三类数据独立定时上报（机器人数据/电机参数/Lora&清扫设置）
//                      新增上报间隔 HTTP 配置接口 GET/POST /api/v1/system/report_intervals
//                      新增 /api/v1/system/version 版本查询接口
// 1.2.0 (2025-xx-xx) - OpenAPI 文档同步更新
// 1.1.0 (2025-xx-xx) - 初始版本

#endif  // VERSION_H_
