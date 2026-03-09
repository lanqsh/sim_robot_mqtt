// 命令发送模块
import * as api from './api.js';
import * as ui from './ui.js';
import { WEEKDAY_NAMES } from './config.js';

function formatProtectionInfoText(protectionInfo, multiLine = false) {
    if (protectionInfo === null || protectionInfo === undefined) {
        return '无';
    }

    if (typeof protectionInfo === 'string') {
        return protectionInfo;
    }

    if (typeof protectionInfo === 'number') {
        return `protection_info=${protectionInfo}`;
    }

    const windProtection = protectionInfo.wind_protection;
    const humidityProtection = protectionInfo.humidity_protection;
    const bracketProtection = protectionInfo.bracket_protection;
    const ambientTemperatureProtection = protectionInfo.ambient_temperature_protection;

    const lines = [
        `大风保护: ${windProtection ? 1 : 0}`,
        `湿度保护: ${humidityProtection ? 1 : 0}`,
        `支架保护: ${bracketProtection ? 1 : 0}`,
        `环境温度保护: ${ambientTemperatureProtection ? 1 : 0}`
    ];

    return multiLine ? lines.join('\n') : lines.join(', ');
}

function renderStartReplyResult(result) {
    const resultEl = document.getElementById('startReplyResult');
    if (!resultEl) return;

    const reply = result?.start_reply;
    if (!reply) {
        resultEl.style.display = 'none';
        resultEl.textContent = '';
        return;
    }

    if (!reply.available) {
        resultEl.style.display = 'block';
        resultEl.textContent = '启动请求已发送，尚未收到启动回复（0xF1）。';
        return;
    }

    const weekdayText = WEEKDAY_NAMES?.[reply.weekday] ?? `星期${reply.weekday}`;
    const timeText = reply.start_time || '';
    const protectionText = formatProtectionInfoText(reply.protection_info, true);

    resultEl.style.display = 'block';
    resultEl.textContent =
`启动回复信息（0xF1）
启动运行标志: ${reply.start_flag}
时间: ${timeText}${timeText ? ` ${weekdayText}` : ''}
当前风速: ${reply.wind_speed}
通信箱数量: ${reply.comm_box_count}
机器人数量: ${reply.robot_count}
后台保护信息: ${protectionText}`;
}

function formatStartReplyText(reply) {
    if (!reply) return 'start_reply: 无';
    if (!reply.available) return 'start_reply: 尚未收到回复';

    const weekdayText = WEEKDAY_NAMES?.[reply.weekday] ?? `星期${reply.weekday}`;
    const timeText = reply.start_time || '';
    const protectionText = formatProtectionInfoText(reply.protection_info);

    return `start_reply:\n` +
        `  启动运行标志: ${reply.start_flag}\n` +
        `  时间: ${timeText}${timeText ? ` ${weekdayText}` : ''}\n` +
        `  当前风速: ${reply.wind_speed}\n` +
        `  通信箱数量: ${reply.comm_box_count}\n` +
        `  机器人数量: ${reply.robot_count}\n` +
        `  后台保护信息: ${protectionText}`;
}

async function pollRequestReply(robotId, requestId, options = {}) {
    const intervalMs = options.intervalMs ?? 1000;
    const timeoutMs = options.timeoutMs ?? 10000;
    const safeIntervalMs = intervalMs > 0 ? intervalMs : 1000;
    const deadline = Date.now() + timeoutMs;

    while (Date.now() < deadline) {
        const status = await api.getRequestReply(robotId, requestId, safeIntervalMs);
        if (status?.success && status?.received) {
            return status;
        }
    }

    return null;
}

// 发送停机位设置请求(A3)
export async function sendParkingPositionRequest(robotId, parkingPosition) {
    if (!robotId) {
        alert('请填写机器人ID');
        return false;
    }

    if (Number.isNaN(parkingPosition) || parkingPosition < 0 || parkingPosition > 255) {
        alert('停机位参数必须在0-255之间');
        return false;
    }

    const robotInfo = `机器人ID: ${robotId}`;
    const confirmMsg = `确定发送停机位设置请求吗？\n\n${robotInfo}\n停机位: ${parkingPosition}`;
    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送停机位设置请求...');

        const result = await api.sendParkingPositionRequest(robotId, {
            parking_position: parkingPosition
        });

        ui.hideLoading();

        if (result.success) {
            alert(`停机位设置请求发送成功！\n\n机器人: ${result.robot_id}\n停机位: ${result.parking_position}\n\n请等待平台回复...`);
            return true;
        }

        alert('发送失败: ' + result.error);
        return false;
    } catch (error) {
        ui.hideLoading();
        console.error('发送停机位设置请求失败:', error);
        alert('发送失败: ' + error.message);
        return false;
    }
}

// 发送定时设置请求(A2)
export async function sendScheduleParamsRequest(robotId, tasks) {
    if (!robotId) {
        alert('请填写机器人ID');
        return false;
    }

    if (!Array.isArray(tasks) || tasks.length !== 7) {
        alert('定时设置参数错误：需要7组定时任务');
        return false;
    }

    for (let i = 0; i < tasks.length; i++) {
        const item = tasks[i];
        if (Number.isNaN(item.weekday) || item.weekday < 0 || item.weekday > 6) {
            alert(`定时${i + 1} 星期必须在0-6之间`);
            return false;
        }
        if (Number.isNaN(item.hour) || item.hour < 0 || item.hour > 23) {
            alert(`定时${i + 1} 小时必须在0-23之间`);
            return false;
        }
        if (Number.isNaN(item.minute) || item.minute < 0 || item.minute > 59) {
            alert(`定时${i + 1} 分钟必须在0-59之间`);
            return false;
        }
        if (Number.isNaN(item.run_count) || item.run_count < 0 || item.run_count > 255) {
            alert(`定时${i + 1} 运行次数必须在0-255之间`);
            return false;
        }
    }

    const robotInfo = `机器人ID: ${robotId}`;
    const activeCount = tasks.filter(t => t.run_count > 0).length;
    const confirmMsg = `确定发送定时设置请求吗？\n\n${robotInfo}\n有效定时组数: ${activeCount}/7`;
    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送定时设置请求...');

        const result = await api.sendScheduleParamsRequest(robotId, { tasks });

        ui.hideLoading();

        if (result.success) {
            alert(`定时设置请求发送成功！\n\n机器人: ${result.robot_id}\n\n请等待平台回复...`);
            return true;
        }

        alert('发送失败: ' + result.error);
        return false;
    } catch (error) {
        ui.hideLoading();
        console.error('发送定时设置请求失败:', error);
        alert('发送失败: ' + error.message);
        return false;
    }
}

// 发送电池参数设置请求
export async function sendBatteryParamsRequest(robotId, params) {
    if (!robotId) {
        alert('请填写机器人ID');
        return false;
    }

    const requiredFields = [
        'protection_current_ma',
        'high_temp_threshold',
        'low_temp_threshold',
        'protection_temp',
        'recovery_temp',
        'protection_voltage',
        'recovery_voltage',
        'protection_battery_level',
        'limit_run_battery_level',
        'recovery_battery_level'
    ];

    for (const field of requiredFields) {
        const value = params[field];
        if (Number.isNaN(value) || value === null || value === undefined) {
            alert(`参数无效: ${field}`);
            return false;
        }
    }

    const robotInfo = `机器人ID: ${robotId}`;
    const confirmMsg = `确定发送电池参数设置请求吗？\n\n${robotInfo}`;
    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送电池参数设置请求...');

        const result = await api.sendBatteryParamsRequest(robotId, params);

        ui.hideLoading();

        if (result.success) {
            alert(`电池参数设置请求发送成功！\n\n机器人: ${result.robot_id}\n\n请等待平台回复...`);
            return true;
        }

        alert('发送失败: ' + result.error);
        return false;
    } catch (error) {
        ui.hideLoading();
        console.error('发送电池参数设置请求失败:', error);
        alert('发送失败: ' + error.message);
        return false;
    }
}

// 发送电机参数设置请求
export async function sendMotorParamsRequest(robotId, params) {
    if (!robotId) {
        alert('请填写机器人ID');
        return false;
    }

    const requiredFields = [
        'walk_motor_speed',
        'brush_motor_speed',
        'windproof_motor_speed',
        'walk_motor_max_current_ma',
        'brush_motor_max_current_ma',
        'windproof_motor_max_current_ma',
        'walk_motor_warning_current_ma',
        'brush_motor_warning_current_ma',
        'windproof_motor_warning_current_ma',
        'walk_motor_mileage_m',
        'brush_motor_timeout_s',
        'windproof_motor_timeout_s',
        'reverse_time_s',
        'protection_angle'
    ];

    for (const field of requiredFields) {
        const value = params[field];
        if (Number.isNaN(value) || value === null || value === undefined) {
            alert(`参数无效: ${field}`);
            return false;
        }
    }

    const robotInfo = `机器人ID: ${robotId}`;
    const confirmMsg = `确定发送电机参数设置请求吗？\n\n${robotInfo}`;
    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送电机参数设置请求...');

        const result = await api.sendMotorParamsRequest(robotId, params);

        ui.hideLoading();

        if (result.success) {
            alert(`电机参数设置请求发送成功！\n\n机器人: ${result.robot_id}\n\n请等待平台回复...`);
            return true;
        }

        alert('发送失败: ' + result.error);
        return false;
    } catch (error) {
        ui.hideLoading();
        console.error('发送电机参数设置请求失败:', error);
        alert('发送失败: ' + error.message);
        return false;
    }
}

// 发送定时启动请求
export async function sendScheduleRequest(robotId, scheduleId, weekday, hour, minute, runCount) {
    // 验证必填字段
    if (!robotId) {
        alert('请填写机器人ID');
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

    const robotInfo = `机器人ID: ${robotId}`;
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

        const result = await api.sendScheduleStartRequest(robotId, {
            schedule_id: scheduleId,
            weekday: weekday,
            hour: hour,
            minute: minute,
            run_count: runCount
        });

        ui.hideLoading();

        if (result.success) {
            ui.showLoading('请求已发送，正在等待平台回复...');
            const polled = await pollRequestReply(result.robot_id, result.request_id);
            ui.hideLoading();

            const startReply = polled?.start_reply ?? result.start_reply;
            alert(`定时启动请求发送成功！\n\n` +
                `机器人: ${result.robot_id}\n` +
                `定时编号: ${result.schedule_id}\n` +
                `星期: ${WEEKDAY_NAMES[result.weekday]}\n` +
                `时间: ${String(result.hour).padStart(2, '0')}:${String(result.minute).padStart(2, '0')}\n` +
                `运行次数: ${result.run_count}次\n\n` +
                `${formatStartReplyText(startReply)}`);
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
export async function sendStartRequest(robotId) {
    if (!robotId) {
        alert('请填写机器人ID');
        return false;
    }

    const robotInfo = `机器人ID: ${robotId}`;
    const confirmMsg = `确定发送启动请求吗？\n\n${robotInfo}`;

    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送启动请求...');

        const result = await api.sendStartRequest(robotId);

        ui.hideLoading();

        if (result.success) {
            renderStartReplyResult(result);

            ui.showLoading('请求已发送，正在等待平台回复...');
            const polled = await pollRequestReply(result.robot_id, result.request_id);
            ui.hideLoading();

            if (polled?.success) {
                renderStartReplyResult(polled);
            }

            alert(`启动请求发送成功！\n\n机器人: ${result.robot_id}\n\n${formatStartReplyText(polled?.start_reply ?? result.start_reply)}`);
            return true;
        } else {
            renderStartReplyResult(null);
            alert('发送失败: ' + result.error);
            return false;
        }
    } catch (error) {
        ui.hideLoading();
        renderStartReplyResult(null);
        console.error('发送启动请求失败:', error);
        alert('发送失败: ' + error.message);
        return false;
    }
}

// 发送校时请求
export async function sendTimeSyncRequest(robotId) {
    if (!robotId) {
        alert('请填写机器人ID');
        return false;
    }

    const robotInfo = `机器人ID: ${robotId}`;
    const confirmMsg = `确定发送校时请求吗？\n\n${robotInfo}`;

    if (!confirm(confirmMsg)) {
        return false;
    }

    try {
        ui.showLoading('正在发送校时请求...');

        const result = await api.sendTimeSyncRequest(robotId);

        ui.hideLoading();

        if (result.success) {
            ui.showLoading('请求已发送，正在等待平台回复...');
            const polled = await pollRequestReply(result.robot_id, result.request_id);
            ui.hideLoading();

            alert(`校时请求发送成功！\n\n机器人: ${result.robot_id}\n\n${formatStartReplyText(polled?.start_reply ?? result.start_reply)}`);
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

    if (!robotId) {
        // 如果没有输入机器人ID，显示所有告警列表但不勾选任何项
        const allCheckboxes = document.querySelectorAll('.alarm-checkboxes input[type="checkbox"]');
        allCheckboxes.forEach(checkbox => checkbox.checked = false);
        return;
    }

    try {
        const result = await api.getRobotAlarms(robotId);

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

