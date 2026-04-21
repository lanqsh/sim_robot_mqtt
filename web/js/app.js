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

// ── 控制指令 ──────────────────────────────────────────────────────────────

let _controlRobotId = null;

// 打开控制指令模态框
window.openControlModal = function(robotId) {
    _controlRobotId = robotId;
    document.getElementById('controlModalRobotId').textContent = robotId;
    document.getElementById('controlModal').style.display = 'flex';
};

// 关闭控制指令模态框
window.closeControlModal = function() {
    document.getElementById('controlModal').style.display = 'none';
    _controlRobotId = null;
};

// 发送控制指令
window.sendControlCmd = async function(code, desc) {
    if (!_controlRobotId) return;
    if (!confirm(`确定发送 ${desc} (0x${code}) 控制指令吗？`)) return;
    try {
        const result = await api.sendControl(_controlRobotId, code);
        if (result.success) {
            alert(`✓ ${desc} 指令已执行`);
        } else {
            alert('执行失败: ' + result.error);
        }
    } catch (error) {
        console.error('控制指令失败:', error);
        alert('执行失败: ' + error.message);
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

// 数据模拟配置
window.toggleSimConfigSection = function() {
    ui.toggleForm('simConfigSectionContent', 'simConfigCollapseIcon');
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

// ── 版本信息弹窗 ─────────────────────────────────────────────────────────
let _versionRobotId = null;

window.openVersionModal = async function(robotId, robotName) {
    _versionRobotId = robotId;
    document.getElementById('versionModalRobotName').textContent = robotName || robotId;
    document.getElementById('versionModalVersion').textContent   = '加载中...';
    document.getElementById('versionModalFileList').innerHTML    = '<span style="color:#999;font-size:14px;">加载中...</span>';
    document.getElementById('versionModal').style.display = 'flex';

    try {
        const result = await api.getRobotVersion(robotId);
        document.getElementById('versionModalVersion').textContent = result.software_version || '-';
        if (!result.files || result.files.length === 0) {
            document.getElementById('versionModalFileList').innerHTML =
                '<span style="color:#999;font-size:14px;">暂无升级包，请将 .bin 文件放入服务器 ./firmware/ 目录</span>';
        } else {
            document.getElementById('versionModalFileList').innerHTML = result.files.map(f => `
                <div style="display:flex;align-items:center;gap:12px;padding:6px 0;border-bottom:1px solid #eee;font-size:14px;">
                    <span style="flex:1;word-break:break-all;">${f.filename}</span>
                    <span style="color:#999;white-space:nowrap;">${(f.size/1024).toFixed(1)} KB</span>
                    <button class="btn btn-secondary btn-sm" data-fname="${f.filename}" onclick="window.downloadFirmwareFile(this.dataset.fname)">&#11015; 下载</button>
                </div>
            `).join('');
        }
    } catch (e) {
        document.getElementById('versionModalVersion').textContent = '-';
        document.getElementById('versionModalFileList').innerHTML =
            `<span style="color:#c00;font-size:14px;">加载失败: ${e.message}</span>`;
    }
};

window.closeVersionModal = function() {
    document.getElementById('versionModal').style.display = 'none';
    _versionRobotId = null;
};

// ── 编辑机器人 ─────────────────────────────────────────────────────────
let _editOldRobotId = null;

window.openEditModal = async function(robotId) {
    _editOldRobotId = robotId;
    // 首先用占位文本，防止旧值沪进
    document.getElementById('editSerialNumber').value  = '';
    document.getElementById('editRobotId').value        = robotId;
    document.getElementById('editRobotName').value      = '';
    document.getElementById('editBracketCount').value   = 0;
    document.getElementById('editRobotEnabled').value   = 'true';
    document.getElementById('editRobotModal').style.display = 'flex';

    try {
        const result = await api.getRobotInfo(robotId);
        if (result.success) {
            document.getElementById('editSerialNumber').value  = result.serial_number ?? '';
            document.getElementById('editRobotId').value       = result.robot_id;
            document.getElementById('editRobotName').value     = result.robot_name || '';
            document.getElementById('editBracketCount').value  = result.bracket_count ?? 0;
            document.getElementById('editRobotEnabled').value  = result.enabled ? 'true' : 'false';
        } else {
            document.getElementById('editSerialNumber').value = '';
        }
    } catch (e) {
        document.getElementById('editSerialNumber').value = '';
    }
};

window.closeEditModal = function() {
    document.getElementById('editRobotModal').style.display = 'none';
    _editOldRobotId = null;
};

window.saveEditRobot = async function() {
    if (!_editOldRobotId) return;
    const newId           = document.getElementById('editRobotId').value.trim();
    const newName         = document.getElementById('editRobotName').value.trim();
    const newEnabled      = document.getElementById('editRobotEnabled').value === 'true';
    const newBracketCount = parseInt(document.getElementById('editBracketCount').value) || 0;
    const snRaw           = document.getElementById('editSerialNumber').value.trim();
    const newSerial       = snRaw !== '' ? parseInt(snRaw) : undefined;

    if (!newId) { alert('机器人 ID 不能为空'); return; }
    if (newSerial !== undefined && (isNaN(newSerial) || newSerial < 1)) {
        alert('序号必须为正整数'); return;
    }

    const payload = { robot_id: newId, robot_name: newName, enabled: newEnabled, bracket_count: newBracketCount };
    if (newSerial !== undefined) payload.serial_number = newSerial;

    try {
        const result = await api.updateRobot(_editOldRobotId, payload);
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

// 打开参数配置弹窗（并发拉取当前配置填充表单）
window.openParamConfig = async function(robotId) {
    _paramConfigRobotId = robotId;
    document.getElementById('paramConfigRobotId').textContent = robotId;
    document.getElementById('paramConfigModal').style.display = 'flex';

    // 并发请求7个GET接口
    const [motorRes, batteryRes, scheduleRes, parkingRes, loraRes, daytimeRes, runtimeRes] = await Promise.allSettled([
        api.getMotorParams(robotId),
        api.getBatteryParams(robotId),
        api.getScheduleParams(robotId),
        api.getParkingPosition(robotId),
        api.getLoraParams(robotId),
        api.getDaytimeScanProtect(robotId),
        api.getRuntimeData(robotId)
    ]);

    const setVal = (id, v) => { const el = document.getElementById(id); if (el && v !== undefined && v !== null) el.value = v; };

    // 电机参数
    if (motorRes.status === 'fulfilled' && motorRes.value?.success) {
        const d = motorRes.value.data;
        setVal('pc_walkMotorSpeed',               d.walk_motor_speed);
        setVal('pc_brushMotorSpeed',              d.brush_motor_speed);
        setVal('pc_windproofMotorSpeed',          d.windproof_motor_speed);
        setVal('pc_walkMotorMaxCurrent',          d.walk_motor_max_current_ma);
        setVal('pc_brushMotorMaxCurrent',         d.brush_motor_max_current_ma);
        setVal('pc_windproofMotorMaxCurrent',     d.windproof_motor_max_current_ma);
        setVal('pc_walkMotorWarningCurrent',      d.walk_motor_warning_current_ma);
        setVal('pc_brushMotorWarningCurrent',     d.brush_motor_warning_current_ma);
        setVal('pc_windproofMotorWarningCurrent', d.windproof_motor_warning_current_ma);
        setVal('pc_walkMotorMileage',             d.walk_motor_mileage_m);
        setVal('pc_brushMotorTimeout',            d.brush_motor_timeout_s);
        setVal('pc_windproofMotorTimeout',        d.windproof_motor_timeout_s);
        setVal('pc_reverseTime',                  d.reverse_time_s);
        setVal('pc_protectionAngle',              d.protection_angle);
    }

    // 电池参数
    if (batteryRes.status === 'fulfilled' && batteryRes.value?.success) {
        const d = batteryRes.value.data;
        setVal('pc_protectionCurrent',      d.protection_current_ma);
        setVal('pc_highTempThreshold',      d.high_temp_threshold);
        setVal('pc_lowTempThreshold',       d.low_temp_threshold);
        setVal('pc_protectionTemp',         d.protection_temp);
        setVal('pc_recoveryTemp',           d.recovery_temp);
        setVal('pc_protectionVoltage',      d.protection_voltage);
        setVal('pc_recoveryVoltage',        d.recovery_voltage);
        setVal('pc_protectionBatteryLevel', d.protection_battery_level);
        setVal('pc_limitRunBatteryLevel',   d.limit_run_battery_level);
        setVal('pc_recoveryBatteryLevel',   d.recovery_battery_level);
    }

    // 定时任务
    if (scheduleRes.status === 'fulfilled' && scheduleRes.value?.success) {
        const tasks = scheduleRes.value.tasks || [];
        tasks.forEach((t, i) => {
            const n = i + 1;
            setVal(`pc_wday${n}`, t.weekday);
            setVal(`pc_hour${n}`, t.hour);
            setVal(`pc_min${n}`,  t.minute);
            setVal(`pc_cnt${n}`,  t.run_count);
        });
    }

    // 停机位
    if (parkingRes.status === 'fulfilled' && parkingRes.value?.success) {
        setVal('pc_parkingPosition', parkingRes.value.parking_position);
    }

    // Lora参数
    if (loraRes.status === 'fulfilled' && loraRes.value?.success) {
        setVal('pc_loraPower',     loraRes.value.power);
        setVal('pc_loraFrequency', loraRes.value.frequency);
        setVal('pc_loraRate',      loraRes.value.rate);
    }

    // 白天防误扫
    if (daytimeRes.status === 'fulfilled' && daytimeRes.value?.success) {
        const enabled = daytimeRes.value.enabled;
        const onEl  = document.getElementById('pc_daytimeScanOn');
        const offEl = document.getElementById('pc_daytimeScanOff');
        if (onEl && offEl) { onEl.checked = !!enabled; offEl.checked = !enabled; }
    }

    // 机器人运行数据（E4字段）
    if (runtimeRes.status === 'fulfilled' && runtimeRes.value?.success) {
        const d = runtimeRes.value.data;
        setVal('pc_e4_mainMotorCurrent',  d.main_motor_current);
        setVal('pc_e4_slaveMotorCurrent', d.slave_motor_current);
        setVal('pc_e4_batteryVoltage',    d.battery_voltage);
        setVal('pc_e4_batteryCurrent',    d.battery_current);
        setVal('pc_e4_batteryStatus',     d.battery_status);
        setVal('pc_e4_batteryLevel',      d.battery_level);
        setVal('pc_e4_batteryTemp',       d.battery_temperature);
        setVal('pc_e4_position',          d.position);
        setVal('pc_e4_workingDuration',   d.working_duration);
        setVal('pc_e4_solarVoltage',      d.solar_voltage);
        setVal('pc_e4_solarCurrent',      d.solar_current);
        setVal('pc_e4_totalRunCount',     d.total_run_count);
        setVal('pc_e4_currentLapCount',   d.current_lap_count);
        setVal('pc_e4_boardTemp',         d.board_temperature);
        setVal('pc_e4_alarmFa',           d.alarm_fa);
        setVal('pc_e4_alarmFb',           d.alarm_fb);
        setVal('pc_e4_alarmFc',           d.alarm_fc);
        setVal('pc_e4_alarmFd',           d.alarm_fd);
    }
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

// 保存 E0 参数
window.saveSnapshotE0 = async function() {
    if (!_snapshotRobotId) return;
    const gv = id => { const el = document.getElementById(id); return el ? el.value : ''; };
    const gn = id => { const v = gv(id); return v !== '' ? Number(v) : undefined; };
    const gb = id => { const el = document.getElementById(id); return el ? el.value === 'true' : undefined; };

    const tasks = Array.from({ length: 7 }, (_, i) => {
        const weekday = [];
        for (let d = 1; d <= 7; d++) {
            const cb = document.getElementById(`snap_e0_task_${i}_day_${d}`);
            if (cb && cb.checked) weekday.push(d);
        }
        return { weekday, time: gv(`snap_e0_task_${i}_time`), run_count: gn(`snap_e0_task_${i}_run_count`) };
    });
    const params = {
        lora_power: gn('snap_e0_lora_power'), lora_frequency: gn('snap_e0_lora_frequency'),
        lora_rate: gn('snap_e0_lora_rate'), robot_number: gv('snap_e0_robot_number'),
        software_version: gv('snap_e0_software_version'), parking_position: gn('snap_e0_parking_position'),
        daytime_scan_protect: gb('snap_e0_daytime_scan_protect'), enabled: gb('snap_e0_enabled'),
        schedule_tasks: tasks,
    };
    try {
        ui.showLoading('正在保存 E0 参数...');
        const r = await api.setReportE0(_snapshotRobotId, params);
        ui.hideLoading();
        r.success ? alert('E0 参数已保存！') : alert('保存失败: ' + r.error);
    } catch (e) { ui.hideLoading(); alert('保存失败: ' + e.message); }
};

// 保存 E1 参数
window.saveSnapshotE1 = async function() {
    if (!_snapshotRobotId) return;
    const gn = id => { const el = document.getElementById(id); return el && el.value !== '' ? Number(el.value) : undefined; };
    const keys = ['walk_motor_speed','brush_motor_speed','windproof_motor_speed',
        'walk_motor_max_current_ma','brush_motor_max_current_ma','windproof_motor_max_current_ma',
        'walk_motor_warning_current_ma','brush_motor_warning_current_ma','windproof_motor_warning_current_ma',
        'walk_motor_mileage_m','brush_motor_timeout_s','windproof_motor_timeout_s',
        'reverse_time_s','protection_angle','protection_current_ma','high_temp_threshold',
        'low_temp_threshold','protection_temp','recovery_temp','protection_voltage','recovery_voltage',
        'protection_battery_level','limit_run_battery_level','recovery_battery_level',
        'board_protection_temp','board_recovery_temp'];
    const params = {};
    keys.forEach(k => { const v = gn(`snap_e1_${k}`); if (v !== undefined) params[k] = v; });
    try {
        ui.showLoading('正在保存 E1 参数...');
        const r = await api.setReportE1(_snapshotRobotId, params);
        ui.hideLoading();
        r.success ? alert('E1 参数已保存！') : alert('保存失败: ' + r.error);
    } catch (e) { ui.hideLoading(); alert('保存失败: ' + e.message); }
};

// 保存 E4 参数
window.saveSnapshotE4 = async function() {
    if (!_snapshotRobotId) return;
    const gn = id => { const el = document.getElementById(id); return el && el.value !== '' ? Number(el.value) : undefined; };
    const gv = id => { const el = document.getElementById(id); return el ? el.value : undefined; };
    const numKeys = ['alarm_fa','alarm_fb','alarm_fc','alarm_fd','main_motor_current','slave_motor_current',
        'battery_voltage','battery_current','battery_status','battery_level','battery_temperature',
        'working_duration','total_run_count','current_lap_count','solar_voltage','solar_current',
        'board_temperature','timestamp_hour','timestamp_minute','timestamp_second'];
    const params = {};
    numKeys.forEach(k => { const v = gn(`snap_e4_${k}`); if (v !== undefined) params[k] = v; });
    const pi = gv('snap_e4_position_info');
    if (pi !== undefined) params.position_info = pi;
    try {
        ui.showLoading('正在保存 E4 参数...');
        const r = await api.setReportE4(_snapshotRobotId, params);
        ui.hideLoading();
        r.success ? alert('E4 参数已保存！') : alert('保存失败: ' + r.error);
    } catch (e) { ui.hideLoading(); alert('保存失败: ' + e.message); }
};

// 保存 E6 参数
window.saveSnapshotE6 = async function() {
    if (!_snapshotRobotId) return;
    const gn = id => { const el = document.getElementById(id); return el ? parseInt(el.value) : NaN; };
    try {
        ui.showLoading('正在保存 E6 参数...');
        const r = await api.setReportE6(_snapshotRobotId, {
            scheduled_not_run_id: gn('snap_e6_id'),
            weekday:   gn('snap_e6_weekday'),
            hour:      gn('snap_e6_hour'),
            minute:    gn('snap_e6_minute'),
            run_count: gn('snap_e6_run_count'),
            scheduled_not_run_reason: gn('snap_e6_reason'),
            e6_alarm:  gn('snap_e6_alarm')
        });
        ui.hideLoading();
        r.success ? alert('E6 参数已保存！') : alert('保存失败: ' + r.error);
    } catch (e) { ui.hideLoading(); alert('保存失败: ' + e.message); }
};

// 保存 E7 参数
window.saveSnapshotE7 = async function() {
    if (!_snapshotRobotId) return;
    const gn = id => { const el = document.getElementById(id); return el ? parseInt(el.value) : NaN; };
    try {
        ui.showLoading('正在保存 E7 参数...');
        const r = await api.setReportE7(_snapshotRobotId, { not_started_reason: gn('snap_e7_reason'), e7_alarm: gn('snap_e7_alarm') });
        ui.hideLoading();
        r.success ? alert('E7 参数已保存！') : alert('保存失败: ' + r.error);
    } catch (e) { ui.hideLoading(); alert('保存失败: ' + e.message); }
};

// 保存 E8 参数
window.saveSnapshotE8 = async function() {
    if (!_snapshotRobotId) return;
    const gn = id => { const el = document.getElementById(id); return el ? parseInt(el.value) : NaN; };
    try {
        ui.showLoading('正在保存 E8 参数...');
        const r = await api.setReportE8(_snapshotRobotId, { startup_confirm_id: gn('snap_e8_id'), not_started_reason: gn('snap_e8_reason'), e8_alarm: gn('snap_e8_alarm') });
        ui.hideLoading();
        r.success ? alert('E8 参数已保存！') : alert('保存失败: ' + r.error);
    } catch (e) { ui.hideLoading(); alert('保存失败: ' + e.message); }
};

// 保存全部参数（E0/E1/E4/E6/E7/E8）
window.saveSnapshotParams = async function() {
    if (!_snapshotRobotId) return;
    try {
        ui.showLoading('正在保存全部参数...');
        await Promise.all([
            window.saveSnapshotE0(), window.saveSnapshotE1(), window.saveSnapshotE4(),
            window.saveSnapshotE6(), window.saveSnapshotE7(), window.saveSnapshotE8(),
        ]);
        ui.hideLoading();
        alert('全部参数已保存，可通过"手动触发"发送对应上报。');
        window.openReportSnapshot(_snapshotRobotId);
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
    // 内联 input/select 行渲染助手
    const _ri = (label, id, type, value, extra = '') =>
        `<tr><td style="color:#666;white-space:nowrap;padding:4px 10px 4px 0;font-size:13px;">${label}</td>` +
        `<td style="padding:4px 0;"><input id="${id}" type="${type}" value="${value ?? ''}" style="width:110px;" ${extra}></td></tr>`;
    const _rs = (label, id, options, selected) => {
        const opts = options.map(([v, t]) =>
            `<option value="${v}" ${String(selected) === String(v) ? 'selected' : ''}>${t}</option>`).join('');
        return `<tr><td style="color:#666;white-space:nowrap;padding:4px 10px 4px 0;font-size:13px;">${label}</td>` +
               `<td style="padding:4px 0;"><select id="${id}" style="width:110px;">${opts}</select></td></tr>`;
    };
    const _ro = (label, value) => _row(label, value); // 只读行

    // ── E0 ──────────────────────────────────────────────────────────────
    const taskRows = Array.from({ length: 7 }, (_, i) => {
        const t = (e0.schedule_tasks || [])[i] || {};
        const weekdayArr = Array.isArray(t.weekday) ? t.weekday : [];
        const boxes = ['一','二','三','四','五','六','日'].map((d, di) => {
            const dayNum = di + 1;
            const checked = weekdayArr.includes(dayNum) ? 'checked' : '';
            return `<label style="margin-right:5px;cursor:pointer;font-size:12px;white-space:nowrap;">` +
                   `<input type="checkbox" id="snap_e0_task_${i}_day_${dayNum}" ${checked}>${d}</label>`;
        }).join('');
        const timeVal = t.time || (t.hour != null ? `${String(t.hour).padStart(2,'0')}:${String(t.minute).padStart(2,'0')}` : '');
        return `<tr>
            <td style="color:#666;font-size:12px;padding:4px 8px 4px 0;white-space:nowrap;">定时${i+1}</td>
            <td style="padding:4px 2px;">${boxes}</td>
            <td style="padding:4px 4px;"><input id="snap_e0_task_${i}_time" type="text" value="${timeVal}" placeholder="HH:MM" style="width:62px;"></td>
            <td style="padding:4px 0;"><input id="snap_e0_task_${i}_run_count" type="number" min="0" max="255" value="${t.run_count ?? 0}" style="width:50px;"></td>
        </tr>`;
    }).join('');

    const e0Html = `
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:#2980b9;margin-bottom:6px;padding:4px 8px;background:#2980b918;border-radius:4px;">E0 — LoRa参数 &amp; 清扫设置</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">
                ${_ri('LoRa 功率', 'snap_e0_lora_power', 'number', e0.lora_power)}
                ${_ri('LoRa 频率', 'snap_e0_lora_frequency', 'number', e0.lora_frequency)}
                ${_ri('LoRa 速率', 'snap_e0_lora_rate', 'number', e0.lora_rate)}
                ${_ri('机器人编号', 'snap_e0_robot_number', 'text', e0.robot_number || '')}
                ${_ri('软件版本', 'snap_e0_software_version', 'text', e0.software_version || '')}
                ${_ri('停机位', 'snap_e0_parking_position', 'number', e0.parking_position)}
                ${_rs('白天防误扫', 'snap_e0_daytime_scan_protect', [['true','开启'],['false','关闭']], e0.daytime_scan_protect)}
                ${_rs('启用状态', 'snap_e0_enabled', [['true','启用'],['false','禁用']], e0.enabled)}
            </table>
            <div style="font-size:12px;color:#666;margin:8px 0 3px;font-weight:600;">定时任务（weekday 勾选生效星期，时间格式 HH:MM，次数）</div>
            <table style="width:100%;border-collapse:collapse;font-size:12px;">
                <thead><tr>
                    <th style="text-align:left;padding:3px 8px 3px 0;color:#888;width:48px;">任务</th>
                    <th style="text-align:left;padding:3px 0;color:#888;">星期</th>
                    <th style="text-align:left;padding:3px 0;color:#888;width:66px;">时间</th>
                    <th style="text-align:left;padding:3px 0;color:#888;width:54px;">次数</th>
                </tr></thead>
                <tbody>${taskRows}</tbody>
            </table>
            <div style="display:flex;justify-content:flex-end;margin-top:10px;">
                <button class="btn btn-success btn-sm" onclick="saveSnapshotE0()">保存 E0</button>
            </div>
        </div>`;

    // ── E1 ──────────────────────────────────────────────────────────────
    const e1Fields = [
        ['行走电机速率', 'snap_e1_walk_motor_speed', e1.walk_motor_speed],
        ['毛刷电机速率', 'snap_e1_brush_motor_speed', e1.brush_motor_speed],
        ['防风电机速率', 'snap_e1_windproof_motor_speed', e1.windproof_motor_speed],
        ['行走上限电流 (mA)', 'snap_e1_walk_motor_max_current_ma', e1.walk_motor_max_current_ma],
        ['毛刷上限电流 (mA)', 'snap_e1_brush_motor_max_current_ma', e1.brush_motor_max_current_ma],
        ['防风上限电流 (mA)', 'snap_e1_windproof_motor_max_current_ma', e1.windproof_motor_max_current_ma],
        ['行走预警电流 (mA)', 'snap_e1_walk_motor_warning_current_ma', e1.walk_motor_warning_current_ma],
        ['毛刷预警电流 (mA)', 'snap_e1_brush_motor_warning_current_ma', e1.brush_motor_warning_current_ma],
        ['防风预警电流 (mA)', 'snap_e1_windproof_motor_warning_current_ma', e1.windproof_motor_warning_current_ma],
        ['行走里程 (m)', 'snap_e1_walk_motor_mileage_m', e1.walk_motor_mileage_m],
        ['毛刷运行超时 (s)', 'snap_e1_brush_motor_timeout_s', e1.brush_motor_timeout_s],
        ['防风运行超时 (s)', 'snap_e1_windproof_motor_timeout_s', e1.windproof_motor_timeout_s],
        ['反转时间 (s)', 'snap_e1_reverse_time_s', e1.reverse_time_s],
        ['保护角度', 'snap_e1_protection_angle', e1.protection_angle],
        ['保护电流 (mA)', 'snap_e1_protection_current_ma', e1.protection_current_ma],
        ['高温阈值', 'snap_e1_high_temp_threshold', e1.high_temp_threshold],
        ['低温阈值', 'snap_e1_low_temp_threshold', e1.low_temp_threshold],
        ['保护温度', 'snap_e1_protection_temp', e1.protection_temp],
        ['恢复温度', 'snap_e1_recovery_temp', e1.recovery_temp],
        ['保护电压', 'snap_e1_protection_voltage', e1.protection_voltage],
        ['恢复电压', 'snap_e1_recovery_voltage', e1.recovery_voltage],
        ['保护电量', 'snap_e1_protection_battery_level', e1.protection_battery_level],
        ['限制运行电量', 'snap_e1_limit_run_battery_level', e1.limit_run_battery_level],
        ['恢复电量', 'snap_e1_recovery_battery_level', e1.recovery_battery_level],
        ['主板保护温度', 'snap_e1_board_protection_temp', e1.board_protection_temp],
        ['主板恢复温度', 'snap_e1_board_recovery_temp', e1.board_recovery_temp],
    ];
    const e1Rows = e1Fields.map(([l, id, v]) => _ri(l, id, 'number', v)).join('');
    const e1Html = `
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:#16a085;margin-bottom:6px;padding:4px 8px;background:#16a08518;border-radius:4px;">E1 — 电机 &amp; 温度电压保护参数</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">${e1Rows}</table>
            <div style="display:flex;justify-content:flex-end;margin-top:10px;">
                <button class="btn btn-success btn-sm" onclick="saveSnapshotE1()">保存 E1</button>
            </div>
        </div>`;

    // ── E4 ──────────────────────────────────────────────────────────────
    const e4Fields = [
        ['FA 告警', 'snap_e4_alarm_fa', e4.alarm_fa],
        ['FB 告警', 'snap_e4_alarm_fb', e4.alarm_fb],
        ['FC 告警', 'snap_e4_alarm_fc', e4.alarm_fc],
        ['FD 告警', 'snap_e4_alarm_fd', e4.alarm_fd],
        ['主电机电流 (×100mA)', 'snap_e4_main_motor_current', e4.main_motor_current],
        ['从电机电流 (×100mA)', 'snap_e4_slave_motor_current', e4.slave_motor_current],
        ['电池电压 (×100mV)', 'snap_e4_battery_voltage', e4.battery_voltage],
        ['电池电流 (×100mA)', 'snap_e4_battery_current', e4.battery_current],
        ['电池状态', 'snap_e4_battery_status', e4.battery_status],
        ['电池电量 (%)', 'snap_e4_battery_level', e4.battery_level],
        ['电池温度', 'snap_e4_battery_temperature', e4.battery_temperature],
        ['工作时长 (s)', 'snap_e4_working_duration', e4.working_duration],
        ['累计运行次数', 'snap_e4_total_run_count', e4.total_run_count],
        ['当前运行圈数', 'snap_e4_current_lap_count', e4.current_lap_count],
        ['光伏电压 (×100mV)', 'snap_e4_solar_voltage', e4.solar_voltage],
        ['光伏电流 (×100mA)', 'snap_e4_solar_current', e4.solar_current],
        ['主板温度', 'snap_e4_board_temperature', e4.board_temperature],
        ['时间戳-时', 'snap_e4_timestamp_hour', e4.timestamp_hour],
        ['时间戳-分', 'snap_e4_timestamp_minute', e4.timestamp_minute],
        ['时间戳-秒', 'snap_e4_timestamp_second', e4.timestamp_second],
    ];
    const e4Rows = e4Fields.map(([l, id, v]) => _ri(l, id, 'number', v)).join('');
    const e4Html = `
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:#e67e22;margin-bottom:6px;padding:4px 8px;background:#e67e2218;border-radius:4px;">E4 — 机器人实时状态</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">
                ${e4Rows}
                <tr><td style="color:#666;white-space:nowrap;padding:4px 10px 4px 0;font-size:13px;">位置信息</td>
                    <td style="padding:4px 0;"><input id="snap_e4_position_info" type="text" value="${e4.position_info || ''}" style="width:110px;"></td></tr>
            </table>
            <div style="display:flex;justify-content:flex-end;margin-top:10px;">
                <button class="btn btn-success btn-sm" onclick="saveSnapshotE4()">保存 E4</button>
            </div>
        </div>`;

    // ── E6/E7/E8 ────────────────────────────────────────────────────────
    const e678Html = `
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:#8e44ad;margin-bottom:6px;padding:4px 8px;background:#8e44ad18;border-radius:4px;">E6 — 定时请求/未运行原因</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">
                ${_ri('定时器编号 (1-7)', 'snap_e6_id', 'number', e6.scheduled_not_run_id, 'min="1" max="7"')}
                ${_ri('星期 (0-6)', 'snap_e6_weekday', 'number', e6.weekday, 'min="0" max="6"')}
                ${_ri('时 (0-23)', 'snap_e6_hour', 'number', e6.hour, 'min="0" max="23"')}
                ${_ri('分 (0-59)', 'snap_e6_minute', 'number', e6.minute, 'min="0" max="59"')}
                ${_ri('运行次数', 'snap_e6_run_count', 'number', e6.run_count, 'min="0"')}
                ${_ri('未运行原因 (0-255)', 'snap_e6_reason', 'number', e6.scheduled_not_run_reason, 'min="0" max="255"')}
                ${_ri('E6 故障信息', 'snap_e6_alarm', 'number', e6.e6_alarm)}
            </table>
            <div style="display:flex;justify-content:flex-end;margin-top:10px;">
                <button class="btn btn-success btn-sm" onclick="saveSnapshotE6()">保存 E6</button>
            </div>
        </div>
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:#c0392b;margin-bottom:6px;padding:4px 8px;background:#c0392b18;border-radius:4px;">E7 — 未启动原因</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">
                ${_ri('未启动原因 (0-255)', 'snap_e7_reason', 'number', e7.not_started_reason, 'min="0" max="255"')}
                ${_ri('E7 故障信息', 'snap_e7_alarm', 'number', e7.e7_alarm)}
            </table>
            <div style="display:flex;justify-content:flex-end;margin-top:10px;">
                <button class="btn btn-success btn-sm" onclick="saveSnapshotE7()">保存 E7</button>
            </div>
        </div>
        <div style="margin-bottom:18px;">
            <div style="font-weight:700;font-size:14px;color:#27ae60;margin-bottom:6px;padding:4px 8px;background:#27ae6018;border-radius:4px;">E8 — 启动请求回复确认</div>
            <table style="width:100%;border-collapse:collapse;font-size:13px;">
                ${_ri('定时器编号 (0-255)', 'snap_e8_id', 'number', e8.startup_confirm_id, 'min="0" max="255"')}
                ${_ri('未启动原因 (0-255)', 'snap_e8_reason', 'number', e8.not_started_reason, 'min="0" max="255"')}
                ${_ri('E8 故障信息', 'snap_e8_alarm', 'number', e8.e8_alarm)}
            </table>
            <div style="display:flex;justify-content:flex-end;margin-top:10px;">
                <button class="btn btn-success btn-sm" onclick="saveSnapshotE8()">保存 E8</button>
            </div>
        </div>`;

    const left = e0Html + e4Html;
    const right = e1Html + e678Html;
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
        const result = await api.saveRuntimeData(_paramConfigRobotId, params);
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

    // 点击控制指令模态框背景关闭
    const controlModal = document.getElementById('controlModal');
    if (controlModal) {
        controlModal.addEventListener('click', (e) => {
            if (e.target.id === 'controlModal') {
                window.closeControlModal();
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

    // 点击数据模拟模态框背景关闭
    const simConfigModal = document.getElementById('simConfigModal');
    if (simConfigModal) {
        simConfigModal.addEventListener('click', (e) => {
            if (e.target.id === 'simConfigModal') {
                window.closeSimConfigModal();
            }
        });
    }

});

// ── 数据模拟配置 ──────────────────────────────────────────────────────────

let _simRobotId = null;
let _simCurrentTab = 'motorData';
// 记录哪些 tab 已经加载过数据，避免重复请求
const _simTabLoaded = { motorData: false, alarmSim: false };

const _simSetField = (id, val) => { const el = document.getElementById(id); if (el) el.value = val; };
const _simSetRadio = (name, isRandom) => {
    document.querySelectorAll(`input[name="${name}"]`).forEach(r => {
        r.checked = (r.value === (isRandom ? 'random' : 'fixed'));
    });
};

async function _loadMotorSimData() {
    const result = await api.getMotorSimConfig();
    if (result.success && result.motor_sim) {
        const s = result.motor_sim;
        const simEnabledEl = document.getElementById('simEnabled');
        if (simEnabledEl) simEnabledEl.checked = s.enabled ?? false;
        _simSetRadio('mainCurrentMode', s.main_current_random);
        _simSetField('mainCurrentMin', s.main_current_min);
        _simSetField('mainCurrentMax', s.main_current_max);
        _simSetField('mainCurrentFixed', s.main_current_fixed);
        _simSetRadio('slaveCurrentMode', s.slave_current_random);
        _simSetField('slaveCurrentMin', s.slave_current_min);
        _simSetField('slaveCurrentMax', s.slave_current_max);
        _simSetField('slaveCurrentFixed', s.slave_current_fixed);
        _simSetRadio('solarVoltageMode', s.solar_voltage_random);
        _simSetField('solarVoltageMin', s.solar_voltage_min);
        _simSetField('solarVoltageMax', s.solar_voltage_max);
        _simSetField('solarVoltageFixed', s.solar_voltage_fixed);
        _simSetRadio('solarCurrentMode', s.solar_current_random);
        _simSetField('solarCurrentMin', s.solar_current_min);
        _simSetField('solarCurrentMax', s.solar_current_max);
        _simSetField('solarCurrentFixed', s.solar_current_fixed);
        _simSetRadio('boardTempMode', s.board_temp_random);
        _simSetField('boardTempMin', s.board_temp_min);
        _simSetField('boardTempMax', s.board_temp_max);
        _simSetField('boardTempFixed', s.board_temp_fixed);
        _simSetRadio('batteryVoltageMode', s.battery_voltage_random);
        _simSetField('batteryVoltageMin', s.battery_voltage_min);
        _simSetField('batteryVoltageMax', s.battery_voltage_max);
        _simSetField('batteryVoltageFixed', s.battery_voltage_fixed);
        _simSetField('batteryDischargeRun', s.battery_discharge_run);
        _simSetField('batteryDischargeStop', s.battery_discharge_stop);
        _simSetField('batteryChargeRate', s.battery_charge_rate);
        _simSetRadio('batteryTempMode', s.battery_temp_random);
        _simSetField('batteryTempMin', s.battery_temp_min);
        _simSetField('batteryTempMax', s.battery_temp_max);
        _simSetField('batteryTempFixed', s.battery_temp_fixed);
    }
}

async function _loadAlarmSimData() {
    const result = await api.getAlarmSimConfig();
    if (result.success && result.alarm_sim) {
        const alm = result.alarm_sim;
        const alarmEn = document.getElementById('alarmSimEnabled');
        if (alarmEn) alarmEn.checked = alm.enabled ?? false;
        _simSetField('alarmDurationMin', alm.duration_min ?? 5);
        _simSetField('alarmDurationMax', alm.duration_max ?? 10);
        _simSetField('alarmFrequency', alm.frequency ?? 2);
        for (let b = 0; b < 32; b++) {
            const cb = document.getElementById(`fcBit${b}`);
            if (cb) cb.checked = alm[`fc_bit_${b}`] ?? false;
        }
    }
}

window.openSimConfigModal = async function(/*robotId*/) {
    _simTabLoaded.motorData = false;
    _simTabLoaded.alarmSim = false;
    document.getElementById('simConfigModal').style.display = 'flex';
    // 切换到第一个 tab，并触发首次加载
    window.switchSimTab('motorData');
};

window.closeSimConfigModal = function() {
    document.getElementById('simConfigModal').style.display = 'none';
};

window.switchSimTab = async function(tab) {
    _simCurrentTab = tab;
    document.querySelectorAll('.sim-tab-btn').forEach(btn => {
        const isActive = btn.dataset.tab === tab;
        btn.classList.toggle('active', isActive);
        btn.style.borderBottom = isActive ? '3px solid #2980b9' : '3px solid transparent';
        btn.style.color = isActive ? '#2980b9' : '#666';
        btn.style.fontWeight = isActive ? '600' : 'normal';
    });
    document.querySelectorAll('.sim-tab-panel').forEach(panel => {
        panel.style.display = panel.dataset.tab === tab ? 'block' : 'none';
    });
    if (_simTabLoaded[tab]) return;
    try {
        if (tab === 'motorData') {
            await _loadMotorSimData();
        } else if (tab === 'alarmSim') {
            await _loadAlarmSimData();
        }
        _simTabLoaded[tab] = true;
    } catch (e) {
        console.error('获取数据模拟配置失败:', e);
    }
};

window.saveSimConfig = async function() {
    const getFloat = (id) => parseFloat(document.getElementById(id)?.value) || 0;
    const getRadio = (name) => {
        const el = document.querySelector(`input[name="${name}"]:checked`);
        return el ? el.value === 'random' : true;
    };
    try {
        if (_simCurrentTab === 'motorData') {
            const motorConfig = {
                enabled:                document.getElementById('simEnabled')?.checked ?? false,
                main_current_random:    getRadio('mainCurrentMode'),
                main_current_min:       getFloat('mainCurrentMin'),
                main_current_max:       getFloat('mainCurrentMax'),
                main_current_fixed:     getFloat('mainCurrentFixed'),
                slave_current_random:   getRadio('slaveCurrentMode'),
                slave_current_min:      getFloat('slaveCurrentMin'),
                slave_current_max:      getFloat('slaveCurrentMax'),
                slave_current_fixed:    getFloat('slaveCurrentFixed'),
                solar_voltage_random:   getRadio('solarVoltageMode'),
                solar_voltage_min:      getFloat('solarVoltageMin'),
                solar_voltage_max:      getFloat('solarVoltageMax'),
                solar_voltage_fixed:    getFloat('solarVoltageFixed'),
                solar_current_random:   getRadio('solarCurrentMode'),
                solar_current_min:      getFloat('solarCurrentMin'),
                solar_current_max:      getFloat('solarCurrentMax'),
                solar_current_fixed:    getFloat('solarCurrentFixed'),
                board_temp_random:      getRadio('boardTempMode'),
                board_temp_min:         getFloat('boardTempMin'),
                board_temp_max:         getFloat('boardTempMax'),
                board_temp_fixed:       getFloat('boardTempFixed'),
                battery_voltage_random: getRadio('batteryVoltageMode'),
                battery_voltage_min:    getFloat('batteryVoltageMin'),
                battery_voltage_max:    getFloat('batteryVoltageMax'),
                battery_voltage_fixed:  getFloat('batteryVoltageFixed'),
                battery_discharge_run:  getFloat('batteryDischargeRun'),
                battery_discharge_stop: getFloat('batteryDischargeStop'),
                battery_charge_rate:    getFloat('batteryChargeRate'),
                battery_temp_random:    getRadio('batteryTempMode'),
                battery_temp_min:       getFloat('batteryTempMin'),
                battery_temp_max:       getFloat('batteryTempMax'),
                battery_temp_fixed:     getFloat('batteryTempFixed'),
            };
            const result = await api.saveMotorSimConfig(motorConfig);
            if (result.success) {
                alert('✓ 电机模拟配置已保存');
            } else {
                alert('保存失败: ' + (result.error || '未知错误'));
            }
        } else if (_simCurrentTab === 'alarmSim') {
            const alarmData = {
                enabled:      document.getElementById('alarmSimEnabled')?.checked ?? false,
                duration_min: parseInt(document.getElementById('alarmDurationMin')?.value) || 5,
                duration_max: parseInt(document.getElementById('alarmDurationMax')?.value) || 10,
                frequency:    parseInt(document.getElementById('alarmFrequency')?.value) || 2,
            };
            for (let b = 0; b < 32; b++) {
                const cb = document.getElementById(`fcBit${b}`);
                alarmData[`fc_bit_${b}`] = !!(cb && cb.checked);
            }
            const result = await api.saveAlarmSimConfig(alarmData);
            if (result.success) {
                alert('✓ 告警模拟配置已保存');
            } else {
                alert('保存失败: ' + (result.error || '未知错误'));
            }
        }
    } catch (e) {
        console.error('保存数据模拟配置失败:', e);
        alert('保存失败: ' + e.message);
    }
};
