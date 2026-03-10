// API 调用模块
import { API_BASE } from './config.js';

// 获取机器人列表（分页 + 可选查询过滤）
export async function fetchRobots(page, pageSize, filters = {}) {
    const params = new URLSearchParams({ page, pageSize });
    if (filters.robot_name)  params.set('robot_name',  filters.robot_name);
    if (filters.robot_id)    params.set('robot_id',    filters.robot_id);
    if (filters.enabled !== undefined && filters.enabled !== '') params.set('enabled', filters.enabled);
    const response = await fetch(`${API_BASE}/api/v1/robots/get?${params}`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.json();
}

// 编辑机器人信息
export async function updateRobot(oldRobotId, data) {
    const response = await fetch(`${API_BASE}/api/v1/robots/update?robot_id=${encodeURIComponent(oldRobotId)}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
    });
    return await response.json();
}

// 添加单个机器人
export async function addRobot(robotName, serialNumber, robotId, enabled = true, bracketCount = 0) {
    const body = {
        robot_name: robotName || '',
        enabled: enabled,
        bracket_count: (bracketCount && bracketCount >= 0) ? parseInt(bracketCount) : 0
    };

    // 如果提供了序号且大于0，才发送序号
    body.serial_number = (serialNumber && serialNumber > 0) ? parseInt(serialNumber) : 0;

    // 如果提供了 robot_id，则加入请求体
    if (robotId && robotId.trim() !== '') {
        body.robot_id = robotId.trim();
    }

    const response = await fetch(`${API_BASE}/api/v1/robots/add`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });
    return await response.json();
}

// 删除机器人
export async function deleteRobot(robotId) {
    const response = await fetch(`${API_BASE}/api/v1/robots/delete?robot_id=${robotId}`, {
        method: 'POST'
    });
    return await response.json();
}

// 获取机器人详细数据
export async function fetchRobotData(robotId) {
    const response = await fetch(`${API_BASE}/api/v1/robots/data?robot_id=${robotId}`);
    return await response.json();
}

// 获取最近100条后端与平台 MQTT 通信记录
export async function fetchMqttMessages(params = {}) {
    const query = new URLSearchParams();
    if (params.robot_id) query.set('robot_id', params.robot_id);
    if (params.category_key) query.set('category_key', params.category_key);
    if (params.command) query.set('command', params.command);
    if (params.direction_key) query.set('direction_key', params.direction_key);

    const response = await fetch(`${API_BASE}/api/v1/robots/mqtt_messages?${query.toString()}`);
    return await response.json();
}

// 批量添加机器人
export async function batchAddRobots(robots) {
    const response = await fetch(`${API_BASE}/api/v1/robots/batch_add`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ robots })
    });
    return await response.json();
}

// 批量删除机器人
export async function batchDeleteRobots(robotIds) {
    const response = await fetch(`${API_BASE}/api/v1/robots/batch_delete`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ robot_ids: robotIds })
    });
    return await response.json();
}

// 发送定时启动请求
export async function sendScheduleStartRequest(robotId, params) {
    const response = await fetch(`${API_BASE}/api/v1/robots/schedule_start?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params)
    });
    return await response.json();
}

// 发送电机参数设置请求
export async function sendMotorParamsRequest(robotId, params) {
    const response = await fetch(`${API_BASE}/api/v1/robots/motor_params?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params)
    });
    return await response.json();
}

// 发送电池参数设置请求
export async function sendBatteryParamsRequest(robotId, params) {
    const response = await fetch(`${API_BASE}/api/v1/robots/battery_params?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params)
    });
    return await response.json();
}

// 发送定时设置请求
export async function sendScheduleParamsRequest(robotId, params) {
    const response = await fetch(`${API_BASE}/api/v1/robots/schedule_params?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params)
    });
    return await response.json();
}

// 发送停机位设置请求
export async function sendParkingPositionRequest(robotId, params) {
    const response = await fetch(`${API_BASE}/api/v1/robots/parking_position?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params)
    });
    return await response.json();
}

// 发送启动请求
export async function sendStartRequest(robotId) {
    const response = await fetch(`${API_BASE}/api/v1/robots/start?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
    });
    return await response.json();
}

// 发送校时请求
export async function sendTimeSyncRequest(robotId) {
    const response = await fetch(`${API_BASE}/api/v1/robots/time_sync?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
    });
    return await response.json();
}

// 查询请求回复状态（F0/F1/F2）
export async function getRequestReply(robotId, requestId, waitMs = 0) {
    const params = new URLSearchParams({ robot_id: robotId });
    if (requestId !== null && requestId !== undefined) {
        params.set('request_id', String(requestId));
    }
    if (waitMs > 0) {
        params.set('wait_ms', String(waitMs));
    }
    const response = await fetch(`${API_BASE}/api/v1/robots/request_reply?${params}`);
    return await response.json();
}

// 设置机器人告警
export async function setRobotAlarms(robotId, alarmData) {
    const response = await fetch(`${API_BASE}/api/v1/robots/set_alarms?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(alarmData)
    });
    return await response.json();
}

// 获取机器人告警
export async function getRobotAlarms(robotId) {
    const response = await fetch(`${API_BASE}/api/v1/robots/get_alarms?robot_id=${robotId}`);
    return await response.json();
}

// 获取后端系统版本
export async function getSystemVersion() {
    const response = await fetch(`${API_BASE}/api/v1/system/version`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.json();
}

// 获取定时上报间隔配置
export async function getReportIntervals() {
    const response = await fetch(`${API_BASE}/api/v1/system/report_intervals`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.json();
}

// 更新定时上报间隔配置
export async function setReportIntervals(robotDataS, motorParamsS, loraCleanS) {
    const response = await fetch(`${API_BASE}/api/v1/system/report_intervals`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            robot_data_report_interval: robotDataS,
            motor_params_report_interval: motorParamsS,
            lora_clean_report_interval: loraCleanS
        })
    });
    return await response.json();
}

// 设置Lora参数（功率/频率/速率）
export async function sendLoraParamsRequest(robotId, params) {
    const response = await fetch(`${API_BASE}/api/v1/robots/lora_params?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params)
    });
    return await response.json();
}

// 设置白天防误扫开关
export async function sendDaytimeScanProtectRequest(robotId, enabled) {
    const response = await fetch(`${API_BASE}/api/v1/robots/daytime_scan_protect?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled })
    });
    return await response.json();
}

// 设置机器人运行数据（E4上报字段），仅写数据库快照，不触发MQTT
export async function sendRobotE4DataRequest(robotId, params) {
    const response = await fetch(`${API_BASE}/api/v1/robots/data?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params)
    });
    return await response.json();
}

// 获取 MQTT 服务配置
export async function getMqttConfig() {
    const response = await fetch(`${API_BASE}/api/v1/system/mqtt_config`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.json();
}

// 更新 MQTT 服务配置并重新连接
export async function setMqttConfig(broker, username, password) {
    const response = await fetch(`${API_BASE}/api/v1/system/mqtt_config`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ broker, username, password })
    });
    return await response.json();
}

// 获取固件目录文件列表
export async function listFirmwareFiles() {
    const response = await fetch(`${API_BASE}/api/v1/system/firmware`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.json();
}

// 设置机器人版本号
export async function setRobotVersion(version) {
    const response = await fetch(`${API_BASE}/api/v1/system/robot_version`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ version })
    });
    return await response.json();
}

// 触发浏览器下载指定固件文件
export function downloadFirmwareFile(filename) {
    const url = `${API_BASE}/api/v1/system/firmware/download?filename=${encodeURIComponent(filename)}`;
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.style.display = 'none';
    document.body.appendChild(a);
    a.click();
    setTimeout(() => document.body.removeChild(a), 1000);
}

// 手动触发指定上报指令（code: "E0"~"E9"）
export async function triggerReport(robotId, code) {
    const response = await fetch(`${API_BASE}/api/v1/robots/trigger_report?robot_id=${encodeURIComponent(robotId)}&code=${code}`, {
        method: 'POST'
    });
    return await response.json();
}
