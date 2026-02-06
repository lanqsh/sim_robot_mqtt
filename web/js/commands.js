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

// 设置告警
export async function setAlarm() {
    const robotId = document.getElementById('alarmRobotId').value.trim();
    const serialNumber = document.getElementById('alarmSerial').value.trim();

    if (!robotId && !serialNumber) {
        alert('请填写机器人ID或序号（二选一）');
        return false;
    }

    // 收集所有告警类型的选中告警位
    const alarmTypes = ['FA', 'FB', 'FC', 'FD'];
    const alarmData = {};
    const alarmSummary = {};
    let totalSelected = 0;

    alarmTypes.forEach(type => {
        const checkboxes = document.querySelectorAll(`#alarm-${type} input[type="checkbox"]`);
        let value = 0;
        let selected = [];

        checkboxes.forEach(checkbox => {
            if (checkbox.checked) {
                const bit = parseInt(checkbox.getAttribute('data-bit'));
                value |= (1 << bit);
                const label = checkbox.parentElement.textContent.trim();
                selected.push(label);
                totalSelected++;
            }
        });

        alarmData[`alarm_f${type[1].toLowerCase()}`] = value;
        alarmSummary[type] = { value, selected };
    });

    if (totalSelected === 0) {
        if (!confirm('未选中任何告警，这将清除所有告警。是否继续？')) {
            return false;
        }
    }

    // 构建确认信息
    const robotInfo = robotId ? `机器人ID: ${robotId}` : `机器人序号: ${serialNumber}`;
    let alarmInfo = totalSelected > 0 ? `已选中 ${totalSelected} 个告警:\n\n` : '清除所有告警\n\n';

    alarmTypes.forEach(type => {
        const summary = alarmSummary[type];
        if (summary.selected.length > 0 || summary.value > 0) {
            alarmInfo += `${type}: 0x${summary.value.toString(16).toUpperCase().padStart(8, '0')}\n`;
            if (summary.selected.length > 0) {
                alarmInfo += summary.selected.map(s => `  • ${s}`).join('\n') + '\n';
            }
            alarmInfo += '\n';
        }
    });

    const confirmMsg = `确定设置告警吗？\n\n${robotInfo}\n\n${alarmInfo}`;

    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在设置告警...');

        const identifier = robotId || serialNumber;
        const identifierType = robotId ? 'id' : 'serial';
        const result = await api.setRobotAlarms(identifier, identifierType, alarmData);

        ui.hideLoading();

        if (result.success) {
            let successMsg = `告警设置成功！\n\n机器人: ${result.robot_id}\n\n`;
            alarmTypes.forEach(type => {
                const summary = alarmSummary[type];
                successMsg += `${type}: 0x${summary.value.toString(16).toUpperCase().padStart(8, '0')}\n`;
            });
            alert(successMsg);
            return true;
        } else {
            alert('设置失败: ' + result.error);
            return false;
        }
    } catch (error) {
        ui.hideLoading();
        console.error('设置告警失败:', error);
        alert('设置失败: ' + error.message);
        return false;
    }
}

// 清除所有告警选择
export function clearAllAlarms() {
    const checkboxes = document.querySelectorAll('.alarm-checkboxes input[type="checkbox"]');
    checkboxes.forEach(checkbox => {
        checkbox.checked = false;
    });
}

// 加载机器人告警数据并更新复选框
export async function loadAlarmData() {
    const robotId = document.getElementById('alarmRobotId').value.trim();
    const serialNumber = document.getElementById('alarmSerial').value.trim();

    console.log('loadAlarmData 被调用, robotId:', robotId, 'serialNumber:', serialNumber);

    if (!robotId && !serialNumber) {
        // 如果没有输入机器人信息，清空所有复选框
        console.log('没有输入机器人信息，清空所有复选框');
        clearAllAlarms();
        return;
    }

    try {
        const identifier = robotId || serialNumber;
        const identifierType = robotId ? 'id' : 'serial';
        console.log('准备调用API, identifier:', identifier, 'type:', identifierType);

        const result = await api.getRobotAlarms(identifier, identifierType);
        console.log('API返回结果:', result);

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

            console.log('告警数据加载成功:', alarmMapping);
        } else {
            console.warn('获取告警数据失败:', result.error);
            alert('获取告警数据失败: ' + (result.error || '未知错误'));
        }
    } catch (error) {
        console.error('加载告警数据失败:', error);
        alert('加载告警数据失败: ' + error.message);
    }
}

// 切换告警标签页
export async function switchAlarmTab(type) {
    console.log('switchAlarmTab 被调用, type:', type);

    // 隐藏所有告警面板
    const panels = document.querySelectorAll('[id^="alarm-"]');
    panels.forEach(panel => {
        panel.style.display = 'none';
    });

    // 显示选中的面板
    const selectedPanel = document.getElementById(`alarm-${type}`);
    if (selectedPanel) {
        selectedPanel.style.display = 'block';
        console.log(`显示面板: alarm-${type}`);
    } else {
        console.error(`未找到面板: alarm-${type}`);
    }

    // 更新标签状态
    const tabs = document.querySelectorAll('.alarm-tab');
    tabs.forEach(tab => {
        tab.classList.remove('active');
        // 检查按钮文本是否匹配当前类型
        if (tab.textContent.includes(`${type}告警`)) {
            tab.classList.add('active');
            console.log('设置active标签:', tab.textContent);
        }
    });

    // 自动加载告警数据
    console.log('准备调用 loadAlarmData');
    await loadAlarmData();
    console.log('loadAlarmData 调用完成');
}

