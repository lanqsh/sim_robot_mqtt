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

// 加载机器人告警数据并更新复选框
export async function loadAlarmData() {
    // 优先使用模态框中存储的机器人信息
    let robotId = window.currentAlarmRobotId || document.getElementById('alarmRobotId')?.value.trim();
    let serialNumber = window.currentAlarmSerial || document.getElementById('alarmSerial')?.value.trim();

    if (!robotId && !serialNumber) {
        // 如果没有输入机器人信息，显示所有告警列表但不勾选任何项
        const allCheckboxes = document.querySelectorAll('.alarm-checkboxes input[type="checkbox"]');
        allCheckboxes.forEach(checkbox => checkbox.checked = false);
        return;
    }

    try {
        const identifier = robotId || serialNumber;
        const identifierType = robotId ? 'id' : 'serial';
        const result = await api.getRobotAlarms(identifier, identifierType);

        if (result.success) {
            // 更新所有告警类型的复选框
            const alarmMapping = {
                'FA': result.alarm_fa || 0,
                'FB': result.alarm_fb || 0,
                'FC': result.alarm_fc || 0,
                'FD': result.alarm_fd || 0
            };

            Object.keys(alarmMapping).forEach(type => {
                const value = alarmMapping[type];
                const checkboxes = document.querySelectorAll(`#alarm-${type} input[type="checkbox"]`);

                checkboxes.forEach(checkbox => {
                    const bit = parseInt(checkbox.getAttribute('data-bit'));
                    checkbox.checked = (value & (1 << bit)) !== 0;
                });
            });
        }
    } catch (error) {
        console.error('加载告警数据失败:', error);
    }
}

// 切换告警标签页
export async function switchAlarmTab(type, container = null) {
    // 确定操作的容器：如果在模态框中，则操作模态框；否则操作页面固定区域
    const isInModal = document.getElementById('alarmModal')?.classList.contains('active');
    const targetContainer = isInModal ? '#alarmModalContent' : '#alarmFormContent';

    // 隐藏所有告警面板（只在目标容器内）
    const panels = document.querySelectorAll(`${targetContainer} [id^="alarm-"]`);
    panels.forEach(panel => {
        panel.style.display = 'none';
    });

    // 显示选中的面板（使用grid布局）
    const selectedPanel = document.querySelector(`${targetContainer} #alarm-${type}`);
    if (selectedPanel) {
        selectedPanel.style.display = 'grid';
    }

    // 更新标签状态（只在目标容器内）
    const tabs = document.querySelectorAll(`${targetContainer} .alarm-tab`);
    tabs.forEach(tab => {
        tab.classList.remove('active');
        // 检查按钮文本是否匹配当前类型
        if (tab.textContent.includes(`${type}告警`)) {
            tab.classList.add('active');
        }
    });

    // 只在页面固定区域时才需要重新加载数据，模态框已经有数据了
    if (!isInModal) {
        await loadAlarmData();
    }
}

