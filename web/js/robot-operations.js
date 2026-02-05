// 机器人操作模块
import * as api from './api.js';
import * as ui from './ui.js';

// 切换机器人启用状态
export async function toggleRobotStatus(robotId, currentStatus, reloadCallback) {
    const newStatus = !currentStatus;
    const action = newStatus ? '启用' : '禁用';

    if (!confirm(`确定要${action}该机器人吗？`)) {
        return;
    }

    try {
        const result = await api.updateRobotStatus(robotId, newStatus);

        if (result.success) {
            alert(result.message);
            reloadCallback();
        } else {
            alert('操作失败: ' + result.error);
        }
    } catch (error) {
        console.error('切换状态失败:', error);
        alert('切换状态失败: ' + error.message);
    }
}

// 添加机器人
export async function addRobot(robotName, serialNumber, reloadCallback) {
    if (!serialNumber || serialNumber < 1) {
        alert('序号必须填写且大于0');
        return false;
    }

    try {
        const result = await api.addRobot(robotName, serialNumber);

        if (result.success) {
            alert('机器人添加成功!');
            reloadCallback();
            return true;
        } else {
            alert(`添加失败: ${result.error}`);
            return false;
        }
    } catch (error) {
        console.error('添加机器人失败:', error);
        alert(`添加失败: ${error.message}`);
        return false;
    }
}

// 删除机器人
export async function deleteRobot(robotId, reloadCallback) {
    if (!confirm(`确定要删除机器人 ${robotId} 吗？`)) {
        return;
    }

    try {
        const result = await api.deleteRobot(robotId);

        if (result.success) {
            alert('机器人删除成功!');
            reloadCallback();
        } else {
            alert(`删除失败: ${result.error}`);
        }
    } catch (error) {
        console.error('删除机器人失败:', error);
        alert(`删除失败: ${error.message}`);
    }
}

// 查看机器人数据
export async function viewRobotData(robotId) {
    ui.renderRobotData({ robot_id: null });
    document.getElementById('robotDetails').innerHTML = '<div class="loading">加载中...</div>';
    document.getElementById('robotModal').classList.add('active');

    try {
        const data = await api.fetchRobotData(robotId);
        ui.renderRobotData(data);
    } catch (error) {
        console.error('获取机器人数据失败:', error);
        document.getElementById('robotDetails').innerHTML = `<div class="error-message">获取数据失败: ${error.message}</div>`;
    }
}

// 批量添加机器人
export async function batchAdd(startSerial, endSerial, namePrefix, reloadCallback) {
    if (!startSerial || !endSerial) {
        alert('请填写起始序号和结束序号');
        return false;
    }

    if (startSerial > endSerial) {
        alert('起始序号不能大于结束序号');
        return false;
    }

    const count = endSerial - startSerial + 1;
    if (!confirm(`确定要批量添加 ${count} 个机器人吗？`)) {
        return false;
    }

    const robots = [];
    for (let i = startSerial; i <= endSerial; i++) {
        robots.push({
            robot_name: `${namePrefix}${i}`,
            serial_number: i,
            enabled: true
        });
    }

    try {
        ui.showLoading(`正在添加 ${count} 个机器人...`);
        const result = await api.batchAddRobots(robots);
        ui.hideLoading();

        if (result.success) {
            alert(`批量添加成功！共添加 ${result.count} 个机器人`);
            reloadCallback();
            return true;
        } else {
            alert('批量添加失败: ' + result.error);
            return false;
        }
    } catch (error) {
        ui.hideLoading();
        console.error('批量添加机器人失败:', error);
        alert('批量添加失败: ' + error.message);
        return false;
    }
}

// 批量删除机器人
export async function batchDelete(startSerial, endSerial, pageSize, reloadCallback) {
    if (!startSerial || !endSerial) {
        alert('请填写起始序号和结束序号');
        return false;
    }

    if (startSerial > endSerial) {
        alert('起始序号不能大于结束序号');
        return false;
    }

    const count = endSerial - startSerial + 1;
    if (!confirm(`确定要批量删除 ${count} 个机器人吗？此操作不可恢复！`)) {
        return false;
    }

    let robot_ids = [];

    try {
        ui.showLoading('正在获取机器人列表...');
        let robots = [];
        let page = 1;

        while (true) {
            const result = await api.fetchRobots(page, pageSize);
            const pageData = result.data || [];
            robots = robots.concat(pageData);

            const pagination = result.pagination;
            if (!pagination || page >= pagination.totalPages) break;
            page += 1;
        }

        robot_ids = robots
            .filter(robot => robot.serial_number >= startSerial && robot.serial_number <= endSerial)
            .map(robot => robot.robot_id);

        if (robot_ids.length === 0) {
            ui.hideLoading();
            alert('在指定序号范围内没有找到机器人');
            return false;
        }
    } catch (error) {
        ui.hideLoading();
        console.error('获取机器人列表失败:', error);
        alert('获取机器人列表失败: ' + error.message);
        return false;
    }

    try {
        ui.showLoading(`正在删除 ${robot_ids.length} 个机器人...`);
        const result = await api.batchDeleteRobots(robot_ids);
        ui.hideLoading();

        if (result.success) {
            alert(`批量删除成功！共删除 ${result.count} 个机器人`);
            reloadCallback();
            return true;
        } else {
            alert('批量删除失败: ' + result.error);
            return false;
        }
    } catch (error) {
        ui.hideLoading();
        console.error('批量删除机器人失败:', error);
        alert('批量删除失败: ' + error.message);
        return false;
    }
}
