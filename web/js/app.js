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

// 加载机器人列表
async function loadRobots() {
    try {
        const result = await api.fetchRobots(paginationManager.getCurrentPage(), paginationManager.getPageSize(), searchFilters);

        paginationManager.updatePagination(result.pagination);

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

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', () => {
    // 添加机器人表单提交
    document.getElementById('addRobotForm').addEventListener('submit', async (e) => {
        e.preventDefault();

        const robotName = document.getElementById('robotName').value.trim();
        const serialNumber = parseInt(document.getElementById('serialNumber').value);
        const robotId = document.getElementById('robotIdInput').value.trim();
        const enabled = document.getElementById('robotEnabled').value === 'true';

        const success = await robotOps.addRobot(robotName, serialNumber, loadRobots, robotId, enabled);
        if (success) {
            document.getElementById('addRobotForm').reset();
            // 重置后确保 enabled 恢复默认值 "true"
            document.getElementById('robotEnabled').value = 'true';
        }
    });

    // 初始加载机器人列表
    loadRobots();

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

});
