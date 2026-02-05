// 命令发送模块
import * as api from './api.js';
import * as ui from './ui.js';
import { WEEKDAY_NAMES } from './config.js';

// 发送定时启动请求
export async function sendScheduleRequest(robotId, serialNumber, scheduleId, weekday, hour, minute, runCount) {
    // 验证必填字段
    if (!robotId && !serialNumber) {
        alert('请填写机器人ID或序号（二选一）');
        return false;
    }

    if (isNaN(scheduleId) || scheduleId < 1 || scheduleId > 255) {
        alert('定时编号必须在1-255之间');
        return false;
    }

    if (isNaN(hour) || hour < 0 || hour > 23) {
        alert('小时必须在0-23之间');
        return false;
    }

    if (isNaN(minute) || minute < 0 || minute > 59) {
        alert('分钟必须在0-59之间');
        return false;
    }

    if (isNaN(runCount) || runCount < 1 || runCount > 255) {
        alert('运行次数必须在1-255之间');
        return false;
    }

    const robotInfo = robotId ? `机器人ID: ${robotId}` : `机器人序号: ${serialNumber}`;
    const confirmMsg = `确定发送定时启动请求吗？\n\n` +
        `${robotInfo}\n` +
        `定时编号: ${scheduleId}\n` +
        `星期: ${WEEKDAY_NAMES[weekday]}\n` +
        `时间: ${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}\n` +
        `运行次数: ${runCount}次`;

    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送定时启动请求...');

        const identifier = robotId || serialNumber;
        const identifierType = robotId ? 'id' : 'serial';
        const result = await api.sendScheduleStartRequest(identifier, identifierType, {
            schedule_id: scheduleId,
            weekday: weekday,
            hour: hour,
            minute: minute,
            run_count: runCount
        });

        ui.hideLoading();

        if (result.success) {
            alert(`定时启动请求发送成功！\n\n` +
                `机器人: ${result.robot_id}\n` +
                `定时编号: ${result.schedule_id}\n` +
                `星期: ${WEEKDAY_NAMES[result.weekday]}\n` +
                `时间: ${String(result.hour).padStart(2, '0')}:${String(result.minute).padStart(2, '0')}\n` +
                `运行次数: ${result.run_count}次\n\n` +
                `请等待平台回复...`);
            return true;
        } else {
            alert('发送失败: ' + result.error);
            return false;
        }
    } catch (error) {
        ui.hideLoading();
        console.error('发送定时启动请求失败:', error);
        alert('发送失败: ' + error.message);
        return false;
    }
}

// 发送启动请求
export async function sendStartRequest(robotId, serialNumber) {
    if (!robotId && !serialNumber) {
        alert('请填写机器人ID或序号（二选一）');
        return false;
    }

    const robotInfo = robotId ? `机器人ID: ${robotId}` : `机器人序号: ${serialNumber}`;
    const confirmMsg = `确定发送启动请求吗？\n\n${robotInfo}`;

    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送启动请求...');

        const identifier = robotId || serialNumber;
        const identifierType = robotId ? 'id' : 'serial';
        const result = await api.sendStartRequest(identifier, identifierType);

        ui.hideLoading();

        if (result.success) {
            alert(`启动请求发送成功！\n\n机器人: ${result.robot_id}\n\n请等待平台回复...`);
            return true;
        } else {
            alert('发送失败: ' + result.error);
            return false;
        }
    } catch (error) {
        ui.hideLoading();
        console.error('发送启动请求失败:', error);
        alert('发送失败: ' + error.message);
        return false;
    }
}

// 发送校时请求
export async function sendTimeSyncRequest(robotId, serialNumber) {
    if (!robotId && !serialNumber) {
        alert('请填写机器人ID或序号（二选一）');
        return false;
    }

    const robotInfo = robotId ? `机器人ID: ${robotId}` : `机器人序号: ${serialNumber}`;
    const confirmMsg = `确定发送校时请求吗？\n\n${robotInfo}`;

    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送校时请求...');

        const identifier = robotId || serialNumber;
        const identifierType = robotId ? 'id' : 'serial';
        const result = await api.sendTimeSyncRequest(identifier, identifierType);

        ui.hideLoading();

        if (result.success) {
            alert(`校时请求发送成功！\n\n机器人: ${result.robot_id}\n\n请等待平台回复...`);
            return true;
        } else {
            alert('发送失败: ' + result.error);
            return false;
        }
    } catch (error) {
        ui.hideLoading();
        console.error('发送校时请求失败:', error);
        alert('发送失败: ' + error.message);
        return false;
    }
}

// 发送机器人数据上报
export async function sendRobotDataReport(robotId, serialNumber) {
    if (!robotId && !serialNumber) {
        alert('请填写机器人ID或序号（二选一）');
        return false;
    }

    const robotInfo = robotId ? `机器人ID: ${robotId}` : `机器人序号: ${serialNumber}`;
    const confirmMsg = `确定发送机器人数据上报吗？\n\n${robotInfo}`;

    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送机器人数据上报...');

        const identifier = robotId || serialNumber;
        const identifierType = robotId ? 'id' : 'serial';
        const result = await api.sendRobotDataReport(identifier, identifierType);

        ui.hideLoading();

        if (result.success) {
            alert(`机器人数据上报发送成功！\n\n机器人: ${result.robot_id}\n\n这是一个上报类指令，平台不回复。`);
            return true;
        } else {
            alert('发送失败: ' + result.error);
            return false;
        }
    } catch (error) {
        ui.hideLoading();
        console.error('发送机器人数据上报失败:', error);
        alert('发送失败: ' + error.message);
        return false;
    }
}
