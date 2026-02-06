// API 调用模块
import { API_BASE } from './config.js';

// 获取机器人列表（分页）
export async function fetchRobots(page, pageSize) {
    const response = await fetch(`${API_BASE}/api/robots?page=${page}&pageSize=${pageSize}`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.json();
}

// 添加单个机器人
export async function addRobot(robotName, serialNumber) {
    const body = {
        robot_name: robotName || ''
    };

    // 如果提供了序号且大于0，才发送序号；否则发送0让后端自动生成
    body.serial_number = (serialNumber && serialNumber > 0) ? parseInt(serialNumber) : 0;

    const response = await fetch(`${API_BASE}/api/robots`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });
    return await response.json();
}

// 删除机器人
export async function deleteRobot(robotId) {
    const response = await fetch(`${API_BASE}/api/robots/${robotId}`, {
        method: 'DELETE'
    });
    return await response.json();
}

// 切换机器人状态
export async function updateRobotStatus(robotId, enabled) {
    const response = await fetch(`${API_BASE}/api/robots/${robotId}/status`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled })
    });
    return await response.json();
}

// 获取机器人详细数据
export async function fetchRobotData(robotId) {
    const response = await fetch(`${API_BASE}/api/robots/${robotId}/data`);
    return await response.json();
}

// 批量添加机器人
export async function batchAddRobots(robots) {
    const response = await fetch(`${API_BASE}/api/robots/batch`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ robots })
    });
    return await response.json();
}

// 批量删除机器人
export async function batchDeleteRobots(robotIds) {
    const response = await fetch(`${API_BASE}/api/robots/batch-delete`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ robot_ids: robotIds })
    });
    return await response.json();
}

// 发送定时启动请求
export async function sendScheduleStartRequest(identifier, type, params) {
    const response = await fetch(`${API_BASE}/api/robots/${identifier}/schedule_start?type=${type}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params)
    });
    return await response.json();
}

// 发送启动请求
export async function sendStartRequest(identifier, type) {
    const response = await fetch(`${API_BASE}/api/robots/${identifier}/start?type=${type}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
    });
    return await response.json();
}

// 发送校时请求
export async function sendTimeSyncRequest(identifier, type) {
    const response = await fetch(`${API_BASE}/api/robots/${identifier}/time_sync?type=${type}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
    });
    return await response.json();
}

// 设置机器人告警
export async function setRobotAlarms(identifier, type, alarmData) {
    const response = await fetch(`${API_BASE}/api/robots/${identifier}/alarms?type=${type}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(alarmData)
    });
    return await response.json();
}
