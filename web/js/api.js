// API 调用模块
import { API_BASE } from './config.js';

// 获取机器人列表（分页）
export async function fetchRobots(page, pageSize) {
    const response = await fetch(`${API_BASE}/api/v1/robots/get?page=${page}&pageSize=${pageSize}`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.json();
}

// 添加单个机器人
export async function addRobot(robotName, serialNumber, robotId) {
    const body = {
        robot_name: robotName || ''
    };

    // 如果提供了序号且大于0，才发送序号；否则发送0让后端自动生成
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

// 切换机器人状态
export async function updateRobotStatus(robotId, enabled) {
    const response = await fetch(`${API_BASE}/api/v1/robots/status?robot_id=${robotId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled })
    });
    return await response.json();
}

// 获取机器人详细数据
export async function fetchRobotData(robotId) {
    const response = await fetch(`${API_BASE}/api/v1/robots/data?robot_id=${robotId}`);
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

// 手动触发指定上报指令（code: "E0"~"E9"）
export async function triggerReport(robotId, code) {
    const response = await fetch(`${API_BASE}/api/v1/robots/trigger_report?robot_id=${encodeURIComponent(robotId)}&code=${code}`, {
        method: 'POST'
    });
    return await response.json();
}
