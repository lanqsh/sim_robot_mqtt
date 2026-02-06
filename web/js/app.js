// 主应用入口
import * as api from './api.js';
import * as ui from './ui.js';
import * as robotOps from './robot-operations.js';
import * as commands from './commands.js';
import { PaginationManager } from './pagination.js';

// 分页管理器
const paginationManager = new PaginationManager();

// 加载机器人列表
async function loadRobots() {
    try {
        const result = await api.fetchRobots(paginationManager.getCurrentPage(), paginationManager.getPageSize());

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

// 全局函数：切换机器人状态
window.toggleRobotStatus = function(robotId, currentStatus) {
    robotOps.toggleRobotStatus(robotId, currentStatus, loadRobots);
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

// 全局函数：切换表单
window.toggleScheduleForm = () => ui.toggleForm('scheduleFormContent', 'scheduleCollapseIcon');
window.toggleStartForm = () => ui.toggleForm('startFormContent', 'startCollapseIcon');
window.toggleTimeSyncForm = () => ui.toggleForm('timeSyncFormContent', 'timeSyncCollapseIcon');
window.toggleAlarmForm = () => ui.toggleForm('alarmFormContent', 'alarmCollapseIcon');
window.toggleAddRobotForm = () => ui.toggleForm('addRobotContent', 'addRobotCollapseIcon');
window.toggleBatchForm = () => ui.toggleForm('batchFormContent', 'batchCollapseIcon');

// 全局函数：发送定时启动请求
window.sendScheduleRequest = async function() {
    const robotId = document.getElementById('scheduleRobotId').value.trim();
    const serialNumber = document.getElementById('scheduleSerial').value.trim();
    const scheduleId = parseInt(document.getElementById('scheduleId').value);
    const weekday = parseInt(document.getElementById('scheduleWeekday').value);
    const hour = parseInt(document.getElementById('scheduleHour').value);
    const minute = parseInt(document.getElementById('scheduleMinute').value);
    const runCount = parseInt(document.getElementById('scheduleRunCount').value);

    await commands.sendScheduleRequest(robotId, serialNumber, scheduleId, weekday, hour, minute, runCount);
};

// 全局函数：发送启动请求
window.sendStartRequest = async function() {
    const robotId = document.getElementById('startRobotId').value.trim();
    const serialNumber = document.getElementById('startSerial').value.trim();

    await commands.sendStartRequest(robotId, serialNumber);
};

// 全局函数：发送校时请求
window.sendTimeSyncRequest = async function() {
    const robotId = document.getElementById('timeSyncRobotId').value.trim();
    const serialNumber = document.getElementById('timeSyncSerial').value.trim();

    await commands.sendTimeSyncRequest(robotId, serialNumber);
};

// 全局函数：设置告警
window.setAlarm = async function() {
    await commands.setAlarm();
};

// 全局函数：清除所有告警选择
window.clearAllAlarms = function() {
    commands.clearAllAlarms();
};

// 全局函数：加载告警数据
window.loadAlarmData = async function() {
    await commands.loadAlarmData();
};

// 全局函数：切换告警标签页
window.switchAlarmTab = async function(type) {
    console.log('window.switchAlarmTab 被调用, type:', type);
    try {
        await commands.switchAlarmTab(type);
    } catch (error) {
        console.error('switchAlarmTab 执行错误:', error);
    }
};

// 全局函数：打开告警设置
window.openAlarmSettings = async function(robotId, serialNumber) {
    try {
        ui.showLoading('正在加载告警设置...');

        // 从后端获取告警值
        const result = await api.getRobotAlarms(robotId, 'id');

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

        const result = await api.setRobotAlarms(window.currentAlarmRobotId, 'id', alarmData);

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

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', () => {
    // 添加机器人表单提交
    document.getElementById('addRobotForm').addEventListener('submit', async (e) => {
        e.preventDefault();

        const robotName = document.getElementById('robotName').value.trim();
        const serialNumber = parseInt(document.getElementById('serialNumber').value);

        const success = await robotOps.addRobot(robotName, serialNumber, loadRobots);
        if (success) {
            document.getElementById('addRobotForm').reset();
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

    // 告警设置输入框变化时自动加载告警数据
    const alarmRobotIdInput = document.getElementById('alarmRobotId');
    const alarmSerialInput = document.getElementById('alarmSerial');

    if (alarmRobotIdInput) {
        alarmRobotIdInput.addEventListener('input', () => {
            // 当输入机器人ID时，清空序号输入框
            if (alarmRobotIdInput.value.trim()) {
                alarmSerialInput.value = '';
            }
        });

        alarmRobotIdInput.addEventListener('blur', async () => {
            if (alarmRobotIdInput.value.trim()) {
                await commands.loadAlarmData();
            } else if (!alarmSerialInput.value.trim()) {
                // 如果两个输入框都为空，清空所有告警选择
                commands.clearAllAlarms();
            }
        });
    }

    if (alarmSerialInput) {
        alarmSerialInput.addEventListener('input', () => {
            // 当输入序号时，清空机器人ID输入框
            if (alarmSerialInput.value.trim()) {
                alarmRobotIdInput.value = '';
            }
        });

        alarmSerialInput.addEventListener('blur', async () => {
            if (alarmSerialInput.value.trim()) {
                await commands.loadAlarmData();
            } else if (!alarmRobotIdInput.value.trim()) {
                // 如果两个输入框都为空，清空所有告警选择
                commands.clearAllAlarms();
            }
        });
    }
});
