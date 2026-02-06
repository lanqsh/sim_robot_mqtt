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

// 全局函数：切换告警标签页
window.switchAlarmTab = function(type) {
    commands.switchAlarmTab(type);
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
});
