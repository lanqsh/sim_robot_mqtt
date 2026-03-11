// 主应用入口
import * as api from './api.js';
import * as ui from './ui.js';
import * as robotOps from './robot-operations.js';
import * as commands from './commands.js';
import { PaginationManager } from './pagination.js';

// 分页管理器
const paginationManager = new PaginationManager();

// 查询过滤状态
let searchFilters = {};
let mqttDefaultRobotId = '';
let mqttDefaultRobotName = '';

const MQTT_COMMAND_OPTIONS = {
    all: ['A0', 'A1', 'A2', 'A3', 'A4', 'A6', 'A7', 'A8', 'B0', 'B1', 'B2', 'B3', 'B4', 'B5', 'B6', 'BA', 'C0', 'C1', 'C2', 'C3', 'C4', 'C5', 'C6', 'D0', 'D1', 'D2', 'E0', 'E1', 'E4', 'E5', 'E6', 'E7', 'E8', 'E9', 'F0', 'F1', 'F2'],
    param: ['A0', 'A1', 'A2', 'A3', 'A4', 'A6', 'A7', 'A8'],
    control: ['B0', 'B1', 'B2', 'B3', 'B4', 'B5', 'B6', 'BA'],
    query: ['C0', 'C1', 'C2', 'C3', 'C4', 'C5', 'C6'],
    report: ['E0', 'E1', 'E4', 'E5', 'E6', 'E7', 'E8', 'E9'],
    request: ['F0', 'F1', 'F2'],
    upgrade: ['D0', 'D1', 'D2']
};

const MQTT_CATEGORY_LABELS = {
    param: '参数设置',
    control: '控制',
    query: '查询',
    report: '数据上报',
    request: '机器人请求',
    upgrade: '远程升级',
    other: '其他'
};

const MQTT_DIRECTION_LABELS = {
    up: '上行',
    down: '下行'
};

// 加载机器人列表
async function loadRobots() {
    try {
        const result = await api.fetchRobots(paginationManager.getCurrentPage(), paginationManager.getPageSize(), searchFilters);

        paginationManager.updatePagination(result.pagination);

        if (Array.isArray(result.data) && result.data.length > 0) {
            mqttDefaultRobotId = result.data[0].robot_id || '';
            mqttDefaultRobotName = result.data[0].robot_name || mqttDefaultRobotId;
            const mqttRobotInput = document.getElementById('mqttRobotIdInput');
            if (mqttRobotInput && !mqttRobotInput.value.trim()) {
                mqttRobotInput.value = mqttDefaultRobotId;
            }
        }

        if (paginationManager.getTotalCount() === 0) {
            ui.showEmptyState();
            ui.updateStatistics(result.statistics);
            return;
        }

        document.getElementById('totalCount').textContent = `共 ${paginationManager.getTotalCount()} 个机器人`;
        ui.updateStatistics(result.statistics);
        ui.renderRobots(result.data);
        paginationManager.renderPagination();
    } catch (error) {
        console.error('加载机器人失败:', error);
        ui.showError(error.message);
    }
}

// 全局函数：跳转到指定页
window.goToPage = function(page) {
    paginationManager.goToPage(page);
    loadRobots();
    document.querySelector('.robots-container').scrollIntoView({ behavior: 'smooth' });
};

// 全局函数：改变每页显示数量
window.changePageSize = function() {
    const newPageSize = parseInt(document.getElementById('pageSize').value);
    paginationManager.changePageSize(newPageSize);
    loadRobots();
};

// 全局函数：删除机器人
window.deleteRobot = function(robotId) {
    robotOps.deleteRobot(robotId, loadRobots);
};

// 全局函数：查看机器人数据
window.viewRobotData = function(robotId) {
    robotOps.viewRobotData(robotId);
};

// 全局函数：批量添加机器人
window.batchAddRobots = async function() {
    const startSerial = parseInt(document.getElementById('batchStartSerial').value);
    const endSerial = parseInt(document.getElementById('batchEndSerial').value);
    const namePrefix = document.getElementById('batchNamePrefix').value.trim() || 'Robot ';

    const success = await robotOps.batchAdd(startSerial, endSerial, namePrefix, loadRobots);
    if (success) {
        document.getElementById('batchStartSerial').value = '';
        document.getElementById('batchEndSerial').value = '';
        document.getElementById('batchNamePrefix').value = '';
    }
};

// 全局函数：批量删除机器人
window.batchDeleteRobots = async function() {
    const startSerial = parseInt(document.getElementById('batchStartSerial').value);
    const endSerial = parseInt(document.getElementById('batchEndSerial').value);

    await robotOps.batchDelete(startSerial, endSerial, paginationManager.getPageSize(), loadRobots);
};

// 全局函数：关闭模态框
window.closeModal = function() {
    ui.closeModal();
};

// ── 手动触发上报 ──────────────────────────────────────────────────────────

let _triggerRobotId = null;

// 打开手动触发模态框
window.openTriggerModal = function(robotId) {
    _triggerRobotId = robotId;
    document.getElementById('triggerModalRobotId').textContent = robotId;
    document.getElementById('triggerModal').style.display = 'flex';
};

// 关闭手动触发模态框
window.closeTriggerModal = function() {
    document.getElementById('triggerModal').style.display = 'none';
    _triggerRobotId = null;
};

// 发送触发请求
window.triggerReport = async function(code, desc) {
    if (!_triggerRobotId) return;
    try {
        const result = await api.triggerReport(_triggerRobotId, code);
        if (result.success) {
            alert(`✓ ${desc} 上报已发送成功`);
        } else {
            alert('发送失败: ' + result.error);
        }
    } catch (error) {
        console.error('触发上报失败:', error);
        alert('发送失败: ' + error.message);
    }
};

// 全局函数：切换表单
window.toggleScheduleForm = () => ui.toggleForm('scheduleFormContent', 'scheduleCollapseIcon');
window.toggleScheduleParamsForm = () => ui.toggleForm('scheduleParamsFormContent', 'scheduleParamsCollapseIcon');
window.toggleParkingPositionForm = () => ui.toggleForm('parkingPositionFormContent', 'parkingPositionCollapseIcon');
window.toggleMotorParamsForm = () => ui.toggleForm('motorParamsFormContent', 'motorParamsCollapseIcon');
window.toggleBatteryParamsForm = () => ui.toggleForm('batteryParamsFormContent', 'batteryParamsCollapseIcon');
window.toggleStartForm = () => ui.toggleForm('startFormContent', 'startCollapseIcon');
window.toggleTimeSyncForm = () => ui.toggleForm('timeSyncFormContent', 'timeSyncCollapseIcon');
window.toggleAddRobotForm = () => ui.toggleForm('addRobotContent', 'addRobotCollapseIcon');
window.toggleBatchForm = () => ui.toggleForm('batchFormContent', 'batchCollapseIcon');

window.openMqttMessages = function(robotId, robotName) {
    mqttDefaultRobotId = robotId || '';
    mqttDefaultRobotName = robotName || robotId || '';

    const robotInput = document.getElementById('mqttRobotIdInput');
    if (robotInput) robotInput.value = mqttDefaultRobotId;

    const nameEl = document.getElementById('mqttRobotName');
    const euiEl = document.getElementById('mqttRobotEui');
    if (nameEl) nameEl.textContent = mqttDefaultRobotName || mqttDefaultRobotId || 'Robot';
    if (euiEl) euiEl.textContent = mqttDefaultRobotId || '--';

    const categoryEl = document.getElementById('mqttCategoryFilter');
    const directionEl = document.getElementById('mqttDirectionFilter');
    const tableBody = document.getElementById('mqttMessagesTableBody');
    if (categoryEl) categoryEl.value = 'all';
    if (directionEl) directionEl.value = 'all';
    window.updateMqttCommandOptions();
    const commandEl = document.getElementById('mqttCommandFilter');
    if (commandEl) commandEl.value = 'all';
    if (tableBody) {
        tableBody.innerHTML = '<tr><td colspan="6" style="text-align:center; color:#666;">请选择筛选条件后点击查询</td></tr>';
    }

    const modal = document.getElementById('mqttMessagesModal');
    if (modal) modal.style.display = 'flex';
};

window.closeMqttMessagesModal = function() {
    const modal = document.getElementById('mqttMessagesModal');
    if (modal) modal.style.display = 'none';
};

window.updateMqttCommandOptions = function() {
    const categoryEl = document.getElementById('mqttCategoryFilter');
    const commandEl = document.getElementById('mqttCommandFilter');
    if (!categoryEl || !commandEl) return;

    const category = categoryEl.value || 'all';
    const commands = MQTT_COMMAND_OPTIONS[category] || [];

    commandEl.innerHTML = '';
    const allOption = document.createElement('option');
    allOption.value = 'all';
    allOption.textContent = '全部';
    commandEl.appendChild(allOption);

    commands.forEach(cmd => {
        const option = document.createElement('option');
        option.value = cmd;
        option.textContent = cmd;
        commandEl.appendChild(option);
    });
};

window.queryMqttMessages = async function() {
    const robotIdInput = document.getElementById('mqttRobotIdInput');
    const categoryEl = document.getElementById('mqttCategoryFilter');
    const commandEl = document.getElementById('mqttCommandFilter');
    const directionEl = document.getElementById('mqttDirectionFilter');
    const tableBody = document.getElementById('mqttMessagesTableBody');

    if (!tableBody) return;

    const robotId = (robotIdInput && robotIdInput.value.trim()) || mqttDefaultRobotId;
    if (!robotId) {
        tableBody.innerHTML = '<tr><td colspan="6" style="text-align:center; color:#666;">请先输入机器人EUI/ID</td></tr>';
        return;
    }

    const categoryKey = categoryEl ? categoryEl.value : 'all';
    const command = commandEl ? commandEl.value : 'all';
    const directionKey = directionEl ? directionEl.value : 'all';

    try {
        const result = await api.fetchMqttMessages({
            robot_id: robotId,
            category_key: categoryKey,
            command,
            direction_key: directionKey
        });

        if (!result.success) {
            tableBody.innerHTML = `<tr><td colspan="6" style="text-align:center; color:#c00;">${result.error || '查询失败'}</td></tr>`;
            return;
        }

        const nameEl = document.getElementById('mqttRobotName');
        const euiEl = document.getElementById('mqttRobotEui');
        if (nameEl) nameEl.textContent = result.robot_name || robotId;
        if (euiEl) euiEl.textContent = result.eui || robotId;

        const list = Array.isArray(result.messages) ? result.messages : [];
        if (list.length === 0) {
            tableBody.innerHTML = '<tr><td colspan="6" style="text-align:center; color:#666;">暂无符合条件的通信记录</td></tr>';
            return;
        }

        tableBody.innerHTML = list.map(item => `
            <tr>
                <td>${item.index ?? ''}</td>
                <td>${MQTT_CATEGORY_LABELS[item.category] || item.category || ''}</td>
                <td>${item.command ?? ''}</td>
                <td>${MQTT_DIRECTION_LABELS[item.direction] || item.direction || ''}</td>
                <td style="max-width: 560px; word-break: break-all;">${item.data ?? ''}</td>
                <td>${item.time ?? ''}</td>
            </tr>
        `).join('');
    } catch (error) {
        tableBody.innerHTML = `<tr><td colspan="6" style="text-align:center; color:#c00;">查询失败: ${error.message}</td></tr>`;
    }
};

// 全局函数：执行查询
window.searchRobots = function() {
    const nameVal = document.getElementById('searchRobotName').value.trim();
    const idVal   = document.getElementById('searchRobotId').value.trim();
    const enVal   = document.getElementById('searchEnabled').value;

    // 三选一：只取第一个非空的栏
    if (nameVal) {
        searchFilters = { robot_name: nameVal };
    } else if (idVal) {
        searchFilters = { robot_id: idVal };
    } else if (enVal !== '') {
        searchFilters = { enabled: enVal };
    } else {
        searchFilters = {};
    }
    paginationManager.goToPage(1);
    loadRobots();
};

// 全局函数：清除查询
window.clearSearch = function() {
    searchFilters = {};
    document.getElementById('searchRobotName').value = '';
    document.getElementById('searchRobotId').value = '';
    document.getElementById('searchEnabled').value = '';
    paginationManager.goToPage(1);
    loadRobots();
};
window.toggleReportIntervalsForm = async function() {
    const content = document.getElementById('reportIntervalsContent');
    const wasHidden = content.style.display === 'none' || content.style.display === '';
    ui.toggleForm('reportIntervalsContent', 'reportIntervalsCollapseIcon');
    if (wasHidden) await window.loadReportIntervals();
};

// MQTT 配置区域开关
window.toggleMqttConfigForm = async function() {
    const content = document.getElementById('mqttConfigContent');
    const wasHidden = content.style.display === 'none' || content.style.display === '';
    ui.toggleForm('mqttConfigContent', 'mqttConfigCollapseIcon');
    if (wasHidden) await window.loadMqttConfig();
};

// 读取当前 MQTT 配置
window.loadMqttConfig = async function() {
    try {
        const result = await api.getMqttConfig();
        if (result.success) {
            document.getElementById('mqttBroker').value   = result.broker   || '';
            document.getElementById('mqttUsername').value = result.username  || '';
            document.getElementById('mqttPassword').value = '';
            _updateMqttStatusBadge(result.connected);
        }
    } catch (e) {
        console.error('获取 MQTT 配置失败:', e);
    }
};

// 保存 MQTT 配置并重连
window.saveMqttConfig = async function() {
    const broker   = document.getElementById('mqttBroker').value.trim();
    const username = document.getElementById('mqttUsername').value.trim();
    const password = document.getElementById('mqttPassword').value;
    if (!broker) { alert('服务地址不能为空'); return; }
    try {
        const result = await api.setMqttConfig(broker, username, password);
        _updateMqttStatusBadge(result.connected);
        if (result.success) {
            document.getElementById('mqttPassword').value = '';
            alert('✓ MQTT 配置已保存并重新连接成功');
        } else {
            alert('配置已保存，但 MQTT 重连失败: ' + (result.message || ''));
        }
    } catch (e) { alert('保存失败: ' + e.message); }
};

function _updateMqttStatusBadge(connected) {
    const badge = document.getElementById('mqttStatusBadge');
    if (!badge) return;
    if (connected) {
        badge.textContent = '✔ 已连接';
        badge.style.background = '#d4edda';
        badge.style.color = '#276032';
    } else {
        badge.textContent = '✖ 未连接';
        badge.style.background = '#fce8e8';
        badge.style.color = '#a93226';
    }
}

// 机器人版本信息 / 固件下载
window.toggleFirmwareForm = async function() {
    const content = document.getElementById('firmwareContent');
    const wasHidden = content.style.display === 'none' || content.style.display === '';
    ui.toggleForm('firmwareContent', 'firmwareCollapseIcon');
    if (wasHidden) await window.loadFirmwareList();
};

window.loadFirmwareList = async function() {
    const versionInput = document.getElementById('firmwareVersionInput');
    const listEl       = document.getElementById('firmwareFileList');

    listEl.innerHTML = '<div style="color:#999;font-size:14px;padding:8px 0;">加载中...</div>';
    try {
        const result = await api.listFirmwareFiles();
        if (versionInput) versionInput.value = result.version || '';
        if (!result.files || result.files.length === 0) {
            listEl.innerHTML = '<div style="color:#999;font-size:14px;padding:8px 0;">暂无升级包，请将 .bin 文件放入服务器 ./firmware/ 目录</div>';
            return;
        }
        listEl.innerHTML = result.files.map(f => `
            <div style="display:flex; align-items:center; gap:16px; padding:6px 0; border-bottom:1px solid #eee; font-size:14px;">
                <span style="font-weight:600; white-space:nowrap;">文件：</span>
                <span style="flex:1; word-break:break-all;">${f.filename}</span>
                <button class="btn btn-secondary btn-sm" data-fname="${f.filename}" onclick="window.downloadFirmwareFile(this.dataset.fname)">&#11015; 下载</button>
            </div>
        `).join('');
    } catch (e) {
        if (document.getElementById('firmwareVersionInput'))
            document.getElementById('firmwareVersionInput').value = '';
        listEl.innerHTML = `<div style="color:#c00;font-size:14px;padding:8px 0;">加载失败: ${e.message}</div>`;
    }
};

window.saveRobotVersion = async function() {
    const input = document.getElementById('firmwareVersionInput');
    const version = input ? input.value.trim() : '';
    try {
        const result = await api.setRobotVersion(version);
        if (result.success) alert('✓ 版本号已保存: ' + (result.version || '(空)'));
        else alert('保存失败: ' + (result.error || ''));
    } catch (e) { alert('保存失败: ' + e.message); }
};

window.downloadFirmwareFile = function(filename) {
    api.downloadFirmwareFile(filename);
};

// 全局函数：读取定时上报间隔配置
window.loadReportIntervals = async function() {
    try {
        const result = await api.getReportIntervals();
        if (result.success) {
            document.getElementById('robotDataInterval').value = result.robot_data_report_interval;
            document.getElementById('motorParamsInterval').value = result.motor_params_report_interval;
            document.getElementById('loraCleanInterval').value = result.lora_clean_report_interval;
        } else {
            alert('读取失败: ' + result.error);
        }
    } catch (error) {
        console.error('读取上报间隔失败:', error);
        alert('读取失败: ' + error.message);
    }
};

// 全局函数：保存定时上报间隔配置
window.saveReportIntervals = async function() {
    const robotDataS    = parseInt(document.getElementById('robotDataInterval').value);
    const motorParamsS  = parseInt(document.getElementById('motorParamsInterval').value);
    const loraCleanS    = parseInt(document.getElementById('loraCleanInterval').value);

    if (isNaN(robotDataS) || isNaN(motorParamsS) || isNaN(loraCleanS)) {
        alert('请填写所有间隔值');
        return;
    }
    if (robotDataS < 10 || motorParamsS < 10 || loraCleanS < 10) {
        alert('间隔最小值为10秒');
        return;
    }

    try {
        ui.showLoading('正在保存上报间隔配置...');
        const result = await api.setReportIntervals(robotDataS, motorParamsS, loraCleanS);
        ui.hideLoading();
        if (result.success) {
            alert(`上报间隔已保存并实时生效！\n\n机器人数据: ${robotDataS}s\n电机参数: ${motorParamsS}s\nLora&清扫设置: ${loraCleanS}s`);
        } else {
            alert('保存失败: ' + result.error);
        }
    } catch (error) {
        ui.hideLoading();
        console.error('保存上报间隔失败:', error);
        alert('保存失败: ' + error.message);
    }
};

// 全局函数：发送电池参数设置请求
window.sendBatteryParamsRequest = async function() {
    const robotId = document.getElementById('batteryRobotId').value.trim();

    const params = {
        protection_current_ma: parseInt(document.getElementById('protectionCurrent').value),
        high_temp_threshold: parseInt(document.getElementById('highTempThreshold').value),
        low_temp_threshold: parseInt(document.getElementById('lowTempThreshold').value),
        protection_temp: parseInt(document.getElementById('protectionTemp').value),
        recovery_temp: parseInt(document.getElementById('recoveryTemp').value),
        protection_voltage: parseInt(document.getElementById('protectionVoltage').value),
        recovery_voltage: parseInt(document.getElementById('recoveryVoltage').value),
        protection_battery_level: parseInt(document.getElementById('protectionBatteryLevel').value),
        limit_run_battery_level: parseInt(document.getElementById('limitRunBatteryLevel').value),
        recovery_battery_level: parseInt(document.getElementById('recoveryBatteryLevel').value)
    };

    await commands.sendBatteryParamsRequest(robotId, params);
};

// 全局函数：发送电机参数设置请求
window.sendMotorParamsRequest = async function() {
    const robotId = document.getElementById('motorRobotId').value.trim();

    const params = {
        walk_motor_speed: parseInt(document.getElementById('walkMotorSpeed').value),
        brush_motor_speed: parseInt(document.getElementById('brushMotorSpeed').value),
        windproof_motor_speed: parseInt(document.getElementById('windproofMotorSpeed').value),
        walk_motor_max_current_ma: parseInt(document.getElementById('walkMotorMaxCurrent').value),
        brush_motor_max_current_ma: parseInt(document.getElementById('brushMotorMaxCurrent').value),
        windproof_motor_max_current_ma: parseInt(document.getElementById('windproofMotorMaxCurrent').value),
        walk_motor_warning_current_ma: parseInt(document.getElementById('walkMotorWarningCurrent').value),
        brush_motor_warning_current_ma: parseInt(document.getElementById('brushMotorWarningCurrent').value),
        windproof_motor_warning_current_ma: parseInt(document.getElementById('windproofMotorWarningCurrent').value),
        walk_motor_mileage_m: parseInt(document.getElementById('walkMotorMileage').value),
        brush_motor_timeout_s: parseInt(document.getElementById('brushMotorTimeout').value),
        windproof_motor_timeout_s: parseInt(document.getElementById('windproofMotorTimeout').value),
        reverse_time_s: parseInt(document.getElementById('reverseTime').value),
        protection_angle: parseInt(document.getElementById('protectionAngle').value)
    };

    await commands.sendMotorParamsRequest(robotId, params);
};

// 全局函数：发送定时启动请求
window.sendScheduleRequest = async function() {
    const robotId = document.getElementById('scheduleRobotId').value.trim();
    const scheduleId = parseInt(document.getElementById('scheduleId').value);
    const weekday = parseInt(document.getElementById('scheduleWeekday').value);
    const hour = parseInt(document.getElementById('scheduleHour').value);
    const minute = parseInt(document.getElementById('scheduleMinute').value);
    const runCount = parseInt(document.getElementById('scheduleRunCount').value);

    await commands.sendScheduleRequest(robotId, scheduleId, weekday, hour, minute, runCount);
};

// 全局函数：发送定时设置请求(A2)
window.sendScheduleParamsRequest = async function() {
    const robotId = document.getElementById('scheduleParamsRobotId').value.trim();

    const tasks = [];
    for (let i = 1; i <= 7; i++) {
        tasks.push({
            weekday: parseInt(document.getElementById(`a2Weekday${i}`).value),
            hour: parseInt(document.getElementById(`a2Hour${i}`).value),
            minute: parseInt(document.getElementById(`a2Minute${i}`).value),
            run_count: parseInt(document.getElementById(`a2RunCount${i}`).value)
        });
    }

    await commands.sendScheduleParamsRequest(robotId, tasks);
};

// 全局函数：发送停机位设置请求(A3)
window.sendParkingPositionRequest = async function() {
    const robotId = document.getElementById('parkingRobotId').value.trim();
    const parkingPosition = parseInt(document.getElementById('parkingPosition').value);

    await commands.sendParkingPositionRequest(robotId, parkingPosition);
};

// 全局函数：发送启动请求
window.sendStartRequest = async function() {
    const robotId = document.getElementById('startRobotId').value.trim();

    await commands.sendStartRequest(robotId);
};

// 全局函数：发送校时请求
window.sendTimeSyncRequest = async function() {
    const robotId = document.getElementById('timeSyncRobotId').value.trim();

    await commands.sendTimeSyncRequest(robotId);
};

// 全局函数：加载告警数据
window.loadAlarmData = async function() {
    await commands.loadAlarmData();
};

// 全局函数：切换告警标签页
window.switchAlarmTab = async function(type) {
    await commands.switchAlarmTab(type);
};

// 全局函数：打开告警设置
window.openAlarmSettings = async function(robotId, serialNumber) {
    try {
        ui.showLoading('正在加载告警设置...');

        // 从后端获取告警值
        const result = await api.getRobotAlarms(robotId);

        if (result.success) {
            // 存储当前机器人信息
            window.currentAlarmRobotId = robotId;
            window.currentAlarmSerial = serialNumber;

            // 更新模态框标题
            document.getElementById('alarmModalRobotInfo').textContent = `#${serialNumber} - ${robotId}`;

            // 渲染告警设置UI
            ui.renderAlarmSettings(result);

            // 打开模态框
            ui.openAlarmModal();
        } else {
            alert('加载告警设置失败: ' + result.error);
        }

        ui.hideLoading();
    } catch (error) {
        ui.hideLoading();
        console.error('打开告警设置失败:', error);
        alert('打开告警设置失败: ' + error.message);
    }
};

// 全局函数：保存告警设置
window.saveAlarmSettings = async function() {
    if (!window.currentAlarmRobotId) {
        alert('无法确定机器人ID');
        return;
    }

    try {
        // 收集所有告警类型的选中告警位
        const alarmTypes = ['FA', 'FB', 'FC', 'FD'];
        const alarmData = {};

        alarmTypes.forEach(type => {
            const checkboxes = document.querySelectorAll(`#alarm-${type} input[type="checkbox"]`);
            let value = 0;

            checkboxes.forEach(checkbox => {
                if (checkbox.checked) {
                    const bit = parseInt(checkbox.getAttribute('data-bit'));
                    value |= (1 << bit);
                }
            });

            alarmData[`alarm_f${type[1].toLowerCase()}`] = value;
        });

        const confirmMsg = `确定保存告警设置吗？\n\nFA: 0x${alarmData.alarm_fa.toString(16).toUpperCase().padStart(8, '0')}\nFB: 0x${alarmData.alarm_fb.toString(16).toUpperCase().padStart(4, '0')}\nFC: 0x${alarmData.alarm_fc.toString(16).toUpperCase().padStart(8, '0')}\nFD: 0x${alarmData.alarm_fd.toString(16).toUpperCase().padStart(4, '0')}`;

        if (!confirm(confirmMsg)) {
            return;
        }

        ui.showLoading('正在保存告警设置...');

        const result = await api.setRobotAlarms(window.currentAlarmRobotId, alarmData);

        ui.hideLoading();

        if (result.success) {
            alert('告警设置保存成功！');
            ui.closeAlarmModal();
        } else {
            alert('保存失败: ' + result.error);
        }
    } catch (error) {
        ui.hideLoading();
        console.error('保存告警设置失败:', error);
        alert('保存失败: ' + error.message);
    }
};

// 全局函数：关闭告警模态框
window.closeAlarmModal = function() {
    ui.closeAlarmModal();
};

// ── 编辑机器人 ─────────────────────────────────────────────────────────
let _editOldRobotId = null;

window.openEditModal = function(robotId, robotName, enabled) {
    _editOldRobotId = robotId;
    document.getElementById('editRobotId').value    = robotId;
    document.getElementById('editRobotName').value  = robotName;
    document.getElementById('editRobotEnabled').value = enabled ? 'true' : 'false';
    document.getElementById('editRobotModal').style.display = 'flex';
};

window.closeEditModal = function() {
    document.getElementById('editRobotModal').style.display = 'none';
    _editOldRobotId = null;
};

window.saveEditRobot = async function() {
    if (!_editOldRobotId) return;
    const newId      = document.getElementById('editRobotId').value.trim();
    const newName    = document.getElementById('editRobotName').value.trim();
    const newEnabled = document.getElementById('editRobotEnabled').value === 'true';

    if (!newId) { alert('机器人ID 不能为空'); return; }

    try {
        const result = await api.updateRobot(_editOldRobotId, {
            robot_id:    newId,
            robot_name:  newName,
            enabled:     newEnabled
        });
        if (result.success) {
            window.closeEditModal();
            loadRobots();
        } else {
            alert('保存失败: ' + result.error);
        }
    } catch (error) {
        alert('保存失败: ' + error.message);
    }
};

// ── 参数配置弹窗 ─────────────────────────────────────────────────────────

let _paramConfigRobotId = null;

// 打开参数配置弹窗
window.openParamConfig = function(robotId) {
    _paramConfigRobotId = robotId;
    document.getElementById('paramConfigRobotId').textContent = robotId;
    document.getElementById('paramConfigModal').style.display = 'flex';
};

// 关闭参数配置弹窗
window.closeParamConfigModal = function() {
    document.getElementById('paramConfigModal').style.display = 'none';
    _paramConfigRobotId = null;
};

// 折叠/展开参数配置项
window.toggleParamSection = function(sectionId, iconId) {
    const section = document.getElementById(sectionId);
    const icon    = document.getElementById(iconId);
    if (!section) return;
    const isHidden = section.style.display === 'none' || section.style.display === '';
    section.style.display = isHidden ? 'block' : 'none';
    if (icon) icon.textContent = isHidden ? '▼' : '▶';
};

// 参数配置 - 发送电机参数
window.pcSendMotorParams = async function() {
    if (!_paramConfigRobotId) return;
    const id = v => { const el = document.getElementById(v); return el ? parseInt(el.value) : NaN; };
    const params = {
        walk_motor_speed:                    id('pc_walkMotorSpeed'),
        brush_motor_speed:                   id('pc_brushMotorSpeed'),
        windproof_motor_speed:               id('pc_windproofMotorSpeed'),
        walk_motor_max_current_ma:           id('pc_walkMotorMaxCurrent'),
        brush_motor_max_current_ma:          id('pc_brushMotorMaxCurrent'),
        windproof_motor_max_current_ma:      id('pc_windproofMotorMaxCurrent'),
        walk_motor_warning_current_ma:       id('pc_walkMotorWarningCurrent'),
        brush_motor_warning_current_ma:      id('pc_brushMotorWarningCurrent'),
        windproof_motor_warning_current_ma:  id('pc_windproofMotorWarningCurrent'),
        walk_motor_mileage_m:                id('pc_walkMotorMileage'),
        brush_motor_timeout_s:               id('pc_brushMotorTimeout'),
        windproof_motor_timeout_s:           id('pc_windproofMotorTimeout'),
        reverse_time_s:                      id('pc_reverseTime'),
        protection_angle:                    id('pc_protectionAngle')
    };
    if (Object.values(params).some(isNaN)) { alert('请填写所有电机参数字段'); return; }
    try {
        ui.showLoading('正在发送电机参数...');
        const result = await api.sendMotorParamsRequest(_paramConfigRobotId, params);
        ui.hideLoading();
        if (result.success) alert('电机参数设置已发送！');
        else alert('发送失败: ' + result.error);
    } catch (e) { ui.hideLoading(); alert('发送失败: ' + e.message); }
};

// 参数配置 - 发送电池参数
window.pcSendBatteryParams = async function() {
    if (!_paramConfigRobotId) return;
    const id = v => { const el = document.getElementById(v); return el ? parseInt(el.value) : NaN; };
    const params = {
        protection_current_ma:     id('pc_protectionCurrent'),
        high_temp_threshold:       id('pc_highTempThreshold'),
        low_temp_threshold:        id('pc_lowTempThreshold'),
        protection_temp:           id('pc_protectionTemp'),
        recovery_temp:             id('pc_recoveryTemp'),
        protection_voltage:        id('pc_protectionVoltage'),
        recovery_voltage:          id('pc_recoveryVoltage'),
        protection_battery_level:  id('pc_protectionBatteryLevel'),
        limit_run_battery_level:   id('pc_limitRunBatteryLevel'),
        recovery_battery_level:    id('pc_recoveryBatteryLevel')
    };
    if (Object.values(params).some(isNaN)) { alert('请填写所有电池参数字段'); return; }
    try {
        ui.showLoading('正在发送电池参数...');
        const result = await api.sendBatteryParamsRequest(_paramConfigRobotId, params);
        ui.hideLoading();
        if (result.success) alert('电池参数设置已发送！');
        else alert('发送失败: ' + result.error);
    } catch (e) { ui.hideLoading(); alert('发送失败: ' + e.message); }
};

// 参数配置 - 发送定时设置（7个任务）
window.pcSendScheduleParams = async function() {
    if (!_paramConfigRobotId) return;
    const tasks = [];
    for (let i = 1; i <= 7; i++) {
        const wday = parseInt(document.getElementById(`pc_wday${i}`).value);
        const hour = parseInt(document.getElementById(`pc_hour${i}`).value);
        const min  = parseInt(document.getElementById(`pc_min${i}`).value);
        const cnt  = parseInt(document.getElementById(`pc_cnt${i}`).value);
        if ([wday, hour, min, cnt].some(isNaN)) { alert(`请填写任务${i}的所有字段`); return; }
        tasks.push({ weekday: wday, hour, minute: min, run_count: cnt });
    }
    try {
        ui.showLoading('正在发送定时设置...');
        const result = await api.sendScheduleParamsRequest(_paramConfigRobotId, { tasks });
        ui.hideLoading();
        if (result.success) alert('定时设置已发送！');
        else alert('发送失败: ' + result.error);
    } catch (e) { ui.hideLoading(); alert('发送失败: ' + e.message); }
};

// 参数配置 - 发送停机位设置
window.pcSendParkingPosition = async function() {
    if (!_paramConfigRobotId) return;
    const pos = parseInt(document.getElementById('pc_parkingPosition').value);
    if (isNaN(pos) || pos < 0 || pos > 255) { alert('请输入有效的停机位（0-255）'); return; }
    try {
        ui.showLoading('正在发送停机位设置...');
        const result = await api.sendParkingPositionRequest(_paramConfigRobotId, { parking_position: pos });
        ui.hideLoading();
        if (result.success) alert('停机位设置已发送！');
        else alert('发送失败: ' + result.error);
    } catch (e) { ui.hideLoading(); alert('发送失败: ' + e.message); }
};

// 参数配置 - 发送Lora参数
window.pcSendLoraParams = async function() {
    if (!_paramConfigRobotId) return;
    const power     = parseInt(document.getElementById('pc_loraPower').value);
    const frequency = parseInt(document.getElementById('pc_loraFrequency').value);
    const rate      = parseInt(document.getElementById('pc_loraRate').value);
    if ([power, frequency, rate].some(isNaN)) { alert('请填写所有Lora参数（功率、频率、速率）'); return; }
    try {
        ui.showLoading('正在发送Lora参数...');
        const result = await api.sendLoraParamsRequest(_paramConfigRobotId, { power, frequency, rate });
        ui.hideLoading();
        if (result.success) alert('Lora参数已发送！机器人将推送E0上报。');
        else alert('发送失败: ' + result.error);
    } catch (e) { ui.hideLoading(); alert('发送失败: ' + e.message); }
};

// 参数配置 - 发送白天防误扫设置
window.pcSendDaytimeScanProtect = async function() {
    if (!_paramConfigRobotId) return;
    const onRadio = document.getElementById('pc_daytimeScanOn');
    const enabled = onRadio && onRadio.checked;
    try {
        ui.showLoading('正在发送白天防误扫设置...');
        const result = await api.sendDaytimeScanProtectRequest(_paramConfigRobotId, enabled);
        ui.hideLoading();
        if (result.success) alert(`白天防误扫已设置为：${enabled ? '开启' : '关闭'}，机器人将推送E0上报。`);
        else alert('发送失败: ' + result.error);
    } catch (e) { ui.hideLoading(); alert('发送失败: ' + e.message); }
};

// ── 上报数据快照 (E0/E1/E4/E6/E7/E8) ────────────────────────────────────

let _snapshotRobotId = null;

// 打开上报数据快照模态框（并发请求六个接口）
window.openReportSnapshot = async function(robotId) {
    _snapshotRobotId = robotId;
    const modal = document.getElementById('reportSnapshotModal');
    const titleEl = document.getElementById('snapshotModalRobotId');
    const bodyEl = document.getElementById('reportSnapshotBody');
    if (!modal || !bodyEl) return;
    if (titleEl) titleEl.textContent = robotId;
    bodyEl.innerHTML = '<div class="loading">加载中...</div>';
    modal.style.display = 'flex';
    try {
        const [r0, r1, r4, r6, r7, r8] = await Promise.all([
            api.fetchReportE0(robotId),
            api.fetchReportE1(robotId),
            api.fetchReportE4(robotId),
            api.fetchReportE6(robotId),
            api.fetchReportE7(robotId),
            api.fetchReportE8(robotId),
        ]);
        if (!r0.success || !r1.success || !r4.success || !r6.success || !r7.success || !r8.success) {
            const err = [r0, r1, r4, r6, r7, r8].find(r => !r.success);
            bodyEl.innerHTML = `<p style="color:#c00;">获取失败: ${err?.error || '未知错误'}</p>`;
            return;
        }
        bodyEl.innerHTML = _renderSnapshotHtml(r0.data, r1.data, r4.data, r6.data, r7.data, r8.data);
    } catch (e) {
        bodyEl.innerHTML = `<p style="color:#c00;">获取失败: ${e.message}</p>`;
    }
};

// 关闭上报数据快照模态框
window.closeReportSnapshotModal = function() {
    const modal = document.getElementById('reportSnapshotModal');
    if (modal) modal.style.display = 'none';
    _snapshotRobotId = null;
};

// 保存 E6/E7/E8 参数（分别调用各自接口）
window.saveSnapshotParams = async function() {
    if (!_snapshotRobotId) return;
    const g = id => { const el = document.getElementById(id); return el ? parseInt(el.value) : NaN; };
    const e6Id     = g('snap_e6_id');
    const e6Reason = g('snap_e6_reason');
    const e7Reason = g('snap_e7_reason');
    const e8Id     = g('snap_e8_id');
    try {
        ui.showLoading('正在保存...');
        const [r6, r7, r8] = await Promise.all([
            api.setReportE6(_snapshotRobotId, { scheduled_not_run_id: e6Id, scheduled_not_run_reason: e6Reason }),
            api.setReportE7(_snapshotRobotId, { not_started_reason: e7Reason }),
            api.setReportE8(_snapshotRobotId, { startup_confirm_id: e8Id }),
        ]);
        ui.hideLoading();
        if (r6.success && r7.success && r8.success) {
            alert('参数已保存，可通过"手动触发"发送对应上报。');
            window.openReportSnapshot(_snapshotRobotId);
        } else {
            const err = [r6, r7, r8].find(r => !r.success);
            alert('保存失败: ' + (err?.error || '未知错误'));
        }
    } catch (e) { ui.hideLoading(); alert('保存失败: ' + e.message); }
};

const _WEEKDAY_NAMES = ['日', '一', '二', '三', '四', '五', '六'];

function _row(label, value) {
    return `<tr><td style="color:#666;white-space:nowrap;padding:4px 10px 4px 0;">${label}</td><td style="padding:4px 0;font-weight:500;">${value}</td></tr>`;
}

function _section(title, color, rows) {
    return `
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:${color};margin-bottom:6px;padding:4px 8px;background:${color}18;border-radius:4px;">${title}</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">${rows}</table>
        </div>`;
}

function _renderSnapshotHtml(e0, e1, e4, e6, e7, e8) {

    // E0
    const tasks = (e0.schedule_tasks || []).map((t, i) =>
        `定时${i+1}: ${t.time ?? `${String(t.hour).padStart(2,'0')}:${String(t.minute).padStart(2,'0')}`} ×${t.run_count}次`
    ).join('<br>');
    const e0Rows = [
        _row('LoRa 功率', e0.lora_power),
        _row('LoRa 频率', e0.lora_frequency),
        _row('LoRa 速率', e0.lora_rate),
        _row('机器人编号', e0.robot_number || '-'),
        _row('软件版本', e0.software_version || '-'),
        _row('停机位', e0.parking_position),
        _row('白天防误扫', e0.daytime_scan_protect ? '开启' : '关闭'),
        _row('定时任务', tasks || '-'),
    ].join('');

    // E1
    const e1Rows = [
        _row('行走电机速率', e1.walk_motor_speed),
        _row('毛刷电机速率', e1.brush_motor_speed),
        _row('防风电机速率', e1.windproof_motor_speed),
        _row('行走上限电流 (mA)', e1.walk_motor_max_current_ma),
        _row('毛刷上限电流 (mA)', e1.brush_motor_max_current_ma),
        _row('防风上限电流 (mA)', e1.windproof_motor_max_current_ma),
        _row('行走预警电流 (mA)', e1.walk_motor_warning_current_ma),
        _row('毛刷预警电流 (mA)', e1.brush_motor_warning_current_ma),
        _row('防风预警电流 (mA)', e1.windproof_motor_warning_current_ma),
        _row('行走里程 (m)', e1.walk_motor_mileage_m),
        _row('毛刷运行超时 (s)', e1.brush_motor_timeout_s),
        _row('防风运行超时 (s)', e1.windproof_motor_timeout_s),
        _row('反转时间 (s)', e1.reverse_time_s),
        _row('保护角度', e1.protection_angle),
        _row('保护电流 (mA)', e1.protection_current_ma),
        _row('高温阈值', e1.high_temp_threshold),
        _row('低温阈值', e1.low_temp_threshold),
        _row('保护温度', e1.protection_temp),
        _row('恢复温度', e1.recovery_temp),
        _row('保护电压', e1.protection_voltage),
        _row('恢复电压', e1.recovery_voltage),
        _row('保护电量', e1.protection_battery_level),
        _row('限制运行电量', e1.limit_run_battery_level),
        _row('恢复电量', e1.recovery_battery_level),
        _row('主板保护温度', e1.board_protection_temp),
        _row('主板恢复温度', e1.board_recovery_temp),
    ].join('');

    // E4
    const ts = `${String(e4.timestamp_hour).padStart(2,'0')}:${String(e4.timestamp_minute).padStart(2,'0')}:${String(e4.timestamp_second).padStart(2,'0')}`;
    const e4Rows = [
        _row('FA 告警', `0x${(e4.alarm_fa >>> 0).toString(16).toUpperCase().padStart(8,'0')}`),
        _row('FB 告警', `0x${(e4.alarm_fb >>> 0).toString(16).toUpperCase().padStart(4,'0')}`),
        _row('FC 告警', `0x${(e4.alarm_fc >>> 0).toString(16).toUpperCase().padStart(8,'0')}`),
        _row('FD 告警', `0x${(e4.alarm_fd >>> 0).toString(16).toUpperCase().padStart(4,'0')}`),
        _row('主电机电流 (×100mA)', e4.main_motor_current),
        _row('从电机电流 (×100mA)', e4.slave_motor_current),
        _row('电池电压 (×100mV)', e4.battery_voltage),
        _row('电池电流 (×100mA)', e4.battery_current),
        _row('电池状态', e4.battery_status),
        _row('电池电量 (%)', e4.battery_level),
        _row('电池温度', e4.battery_temperature),
        _row('位置信息', e4.position_info || '-'),
        _row('工作时长 (s)', e4.working_duration),
        _row('累计运行次数', e4.total_run_count),
        _row('当前运行圈数', e4.current_lap_count),
        _row('光伏电压 (×100mV)', e4.solar_voltage),
        _row('光伏电流 (×100mA)', e4.solar_current),
        _row('主板温度', e4.board_temperature),
        _row('当前时间戳', ts),
    ].join('');

    // E6/E7/E8 可编辑区
    const e678Html = `
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:#8e44ad;margin-bottom:6px;padding:4px 8px;background:#8e44ad18;border-radius:4px;">E6 — 定时请求/未运行原因</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">
                ${_row('定时器编号 (1-7)', `<input id="snap_e6_id" type="number" min="1" max="7" value="${e6.scheduled_not_run_id}" style="width:80px;">`)}
                ${_row('未运行原因 (0x..)', `<input id="snap_e6_reason" type="number" min="0" max="255" value="${e6.scheduled_not_run_reason}" style="width:80px;">`)}
                ${_row('FA 告警', `0x${(e6.alarm_fa >>> 0).toString(16).toUpperCase().padStart(8,'0')}`)}
            </table>
        </div>
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:#c0392b;margin-bottom:6px;padding:4px 8px;background:#c0392b18;border-radius:4px;">E7 — 未启动原因</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">
                ${_row('未启动原因 (0x..)', `<input id="snap_e7_reason" type="number" min="0" max="255" value="${e7.not_started_reason}" style="width:80px;">`)}
                ${_row('FA 告警', `0x${(e7.alarm_fa >>> 0).toString(16).toUpperCase().padStart(8,'0')}`)}
            </table>
        </div>
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:#27ae60;margin-bottom:6px;padding:4px 8px;background:#27ae6018;border-radius:4px;">E8 — 启动请求回复确认</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">
                ${_row('定时器编号 (1-7)', `<input id="snap_e8_id" type="number" min="0" max="255" value="${e8.startup_confirm_id}" style="width:80px;">`)}
                ${_row('未启动原因', e8.not_started_reason ?? '-')}
                ${_row('故障信息', `0x${(e8.e8_alarm_fa >>> 0).toString(16).toUpperCase().padStart(8, '0')}`)}
            </table>
        </div>`;

    const left = _section('E0 — LoRa参数 &amp; 清扫设置', '#2980b9', e0Rows)
               + _section('E4 — 机器人实时状态', '#e67e22', e4Rows);
    const right = _section('E1 — 电机 &amp; 温度电压保护参数', '#16a085', e1Rows)
                + e678Html;

    return `<div style="display:grid;grid-template-columns:1fr 1fr;gap:20px;align-items:start;">${left}${right}</div>`;
}


// 参数配置 - 设置机器人运行数据（E4字段，仅写数据库）
window.pcSendRobotE4Data = async function() {
    if (!_paramConfigRobotId) return;
    const numVal = (id) => {
        const el = document.getElementById(id);
        if (!el || el.value === '') return undefined;
        const n = parseInt(el.value);
        return isNaN(n) ? undefined : n;
    };
    const params = {};
    const mapping = {
        main_motor_current:  'pc_e4_mainMotorCurrent',
        slave_motor_current: 'pc_e4_slaveMotorCurrent',
        battery_voltage:     'pc_e4_batteryVoltage',
        battery_current:     'pc_e4_batteryCurrent',
        battery_status:      'pc_e4_batteryStatus',
        battery_level:       'pc_e4_batteryLevel',
        battery_temperature: 'pc_e4_batteryTemp',
        position:            'pc_e4_position',
        working_duration:    'pc_e4_workingDuration',
        solar_voltage:       'pc_e4_solarVoltage',
        solar_current:       'pc_e4_solarCurrent',
        total_run_count:     'pc_e4_totalRunCount',
        current_lap_count:   'pc_e4_currentLapCount',
        board_temperature:   'pc_e4_boardTemp',
        alarm_fa:            'pc_e4_alarmFa',
        alarm_fb:            'pc_e4_alarmFb',
        alarm_fc:            'pc_e4_alarmFc',
        alarm_fd:            'pc_e4_alarmFd'
    };
    for (const [key, elemId] of Object.entries(mapping)) {
        const v = numVal(elemId);
        if (v !== undefined) params[key] = v;
    }
    if (Object.keys(params).length === 0) { alert('请至少填写一个字段'); return; }
    try {
        ui.showLoading('正在保存运行数据...');
        const result = await api.sendRobotE4DataRequest(_paramConfigRobotId, params);
        ui.hideLoading();
        if (result.success) alert('机器人运行数据已保存到数据库！');
        else alert('保存失败: ' + result.error);
    } catch (e) { ui.hideLoading(); alert('保存失败: ' + e.message); }
};

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', () => {
    // 添加机器人表单提交
    document.getElementById('addRobotForm').addEventListener('submit', async (e) => {
        e.preventDefault();

        const robotName = document.getElementById('robotName').value.trim();
        const serialNumber = parseInt(document.getElementById('serialNumber').value);
        const robotId = document.getElementById('robotIdInput').value.trim();
        const enabled = document.getElementById('robotEnabled').value === 'true';
        const bracketCount = parseInt(document.getElementById('bracketCount').value) || 0;

        const success = await robotOps.addRobot(robotName, serialNumber, loadRobots, robotId, enabled, bracketCount);
        if (success) {
            document.getElementById('addRobotForm').reset();
            // 重置后确保 enabled 恢复默认值 "true"
            document.getElementById('robotEnabled').value = 'true';
        }
    });

    // 初始加载机器人列表
    loadRobots();
    window.updateMqttCommandOptions();

    // 每10秒刷新一次列表
    setInterval(loadRobots, 10000);

    // 点击模态框背景关闭
    document.getElementById('robotModal').addEventListener('click', (e) => {
        if (e.target.id === 'robotModal') {
            ui.closeModal();
        }
    });

    // 点击告警模态框背景关闭
    const alarmModal = document.getElementById('alarmModal');
    if (alarmModal) {
        alarmModal.addEventListener('click', (e) => {
            if (e.target.id === 'alarmModal') {
                ui.closeAlarmModal();
            }
        });
    }

    // 点击手动触发模态框背景关闭
    const triggerModal = document.getElementById('triggerModal');
    if (triggerModal) {
        triggerModal.addEventListener('click', (e) => {
            if (e.target.id === 'triggerModal') {
                window.closeTriggerModal();
            }
        });
    }

    // 点击编辑模态框背景关闭
    const editRobotModal = document.getElementById('editRobotModal');
    if (editRobotModal) {
        editRobotModal.addEventListener('click', (e) => {
            if (e.target.id === 'editRobotModal') {
                window.closeEditModal();
            }
        });
    }

    // 点击参数配置模态框背景关闭
    const paramConfigModal = document.getElementById('paramConfigModal');
    if (paramConfigModal) {
        paramConfigModal.addEventListener('click', (e) => {
            if (e.target.id === 'paramConfigModal') {
                window.closeParamConfigModal();
            }
        });
    }

    // 点击 MQTT 通信记录模态框背景关闭
    const mqttMessagesModal = document.getElementById('mqttMessagesModal');
    if (mqttMessagesModal) {
        mqttMessagesModal.addEventListener('click', (e) => {
            if (e.target.id === 'mqttMessagesModal') {
                window.closeMqttMessagesModal();
            }
        });
    }

});
