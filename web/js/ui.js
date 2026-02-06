// UI 渲染模块
import { LABEL_MAP } from './config.js';

// 更新统计信息
export function updateStatistics(statistics) {
    if (!statistics) {
        document.getElementById('statTotal').textContent = 0;
        document.getElementById('statEnabled').textContent = 0;
        document.getElementById('statDisabled').textContent = 0;
        return;
    }

    document.getElementById('statTotal').textContent = statistics.total;
    document.getElementById('statEnabled').textContent = statistics.enabled;
    document.getElementById('statDisabled').textContent = statistics.disabled;
}

// 渲染机器人列表
export function renderRobots(robots) {
    const container = document.getElementById('robotsList');
    container.innerHTML = '<div class="robots-grid"></div>';
    const grid = container.querySelector('.robots-grid');

    robots.forEach(robot => {
        const card = document.createElement('div');
        card.className = 'robot-card';
        card.innerHTML = `
            <div class="robot-header">
                <div class="robot-name">
                    <span class="serial-badge">#${robot.serial_number}</span>
                    ${robot.robot_name || '未命名'}
                </div>
                <div class="robot-status ${robot.enabled ? 'enabled' : 'disabled'}">
                    ${robot.enabled ? '已启用' : '已禁用'}
                </div>
            </div>
            <div class="robot-id">ID: ${robot.robot_id}</div>
            <div class="robot-actions">
                <button class="btn ${robot.enabled ? 'btn-warning' : 'btn-success'} ${!robot.enabled ? 'disabled-btn' : ''}"
                        onclick="window.toggleRobotStatus('${robot.robot_id}', ${robot.enabled})">
                    ${robot.enabled ? '禁用' : '启用'}
                </button>
                <button class="btn btn-primary" onclick="window.viewRobotData('${robot.robot_id}')">
                    查看数据
                </button>
                <button class="btn btn-danger" onclick="window.deleteRobot('${robot.robot_id}')">
                    删除
                </button>
            </div>
        `;
        grid.appendChild(card);
    });
}

// 显示空状态
export function showEmptyState() {
    const container = document.getElementById('robotsList');
    container.innerHTML = `
        <div class="empty-state">
            <h3>暂无机器人</h3>
            <p>请添加新机器人开始管理</p>
        </div>
    `;
    document.getElementById('pagination').innerHTML = '';
    document.getElementById('totalCount').textContent = '';
}

// 显示错误信息
export function showError(message) {
    const container = document.getElementById('robotsList');
    container.innerHTML = `
        <div class="error-message">加载机器人列表失败: ${message}</div>
    `;
}

// 显示加载提示
export function showLoading(text = '处理中...') {
    document.getElementById('loadingText').textContent = text;
    document.getElementById('loadingModal').classList.add('active');
}

// 隐藏加载提示
export function hideLoading() {
    document.getElementById('loadingModal').classList.remove('active');
}

// 关闭模态框
export function closeModal() {
    document.getElementById('robotModal').classList.remove('active');
}

// 格式化时间对象为字符串
function formatTimeObject(timeObj) {
    if (!timeObj || typeof timeObj !== 'object') return '';
    const parts = [];
    if (timeObj.year !== undefined) {
        parts.push(`${timeObj.year}年${timeObj.month}月${timeObj.day}日`);
    }
    if (timeObj.hour !== undefined) {
        const h = String(timeObj.hour).padStart(2, '0');
        const m = String(timeObj.minute).padStart(2, '0');
        const s = String(timeObj.second).padStart(2, '0');
        parts.push(`${h}:${m}:${s}`);
    }
    if (timeObj.weekday !== undefined && !timeObj.year) {
        parts.push(`星期${timeObj.weekday}`);
    }
    return parts.join(' ');
}

// 简单字段格式化
function formatValue(key, val) {
    if (val === null) return 'null';
    if (key === 'battery_level') return `${val}%`;
    if (key === 'battery_voltage' || key === 'solar_voltage') {
        return `${(val / 10).toFixed(1)}V`;
    }
    if (key === 'battery_current' || key === 'main_motor_current' || key === 'slave_motor_current' || key === 'solar_current') {
        return `${(val / 10).toFixed(1)}A`;
    }
    if (key === 'battery_temperature' || key === 'board_temperature') {
        return `${val}°C`;
    }
    if (key === 'working_duration') return `${val} 小时`;
    if (typeof val === 'boolean') return val ? '是' : '否';
    return String(val);
}

// 递归渲染对象为HTML
function renderObject(obj) {
    if (obj === null) return '<span class="data-value">null</span>';
    if (typeof obj !== 'object') {
        return `<span class="data-value">${obj}</span>`;
    }
    if (Array.isArray(obj)) {
        let html = '<div class="array-list">';
        obj.forEach((item, idx) => {
            html += `<div class="array-item"><span class="array-index">[${idx}]</span> ${renderObject(item)}</div>`;
        });
        html += '</div>';
        return html;
    }
    let html = '<div class="object-list">';
    for (const key of Object.keys(obj)) {
        const label = LABEL_MAP[key] || key;
        const value = obj[key];
        if (value !== null && typeof value === 'object') {
            html += `<div class="data-item"><span class="data-label">${label}:</span> ${renderObject(value)}</div>`;
        } else {
            html += `<div class="data-item"><span class="data-label">${label}:</span> <span class="data-value">${formatValue(key, value)}</span></div>`;
        }
    }
    html += '</div>';
    return html;
}

// 渲染机器人详细数据
export function renderRobotData(data) {
    const details = document.getElementById('robotDetails');
    const modal = document.getElementById('robotModal');

    if (!data.robot_id) {
        details.innerHTML = `<div class="error-message">${data.error || '无法获取机器人数据'}</div>`;
        return;
    }

    const lastData = typeof data.last_data === 'string' ? JSON.parse(data.last_data) : data.last_data;

    let html = '';
    html += `<div class="data-item"><span class="data-label">机器人ID:</span><span class="data-value">${data.robot_id}</span></div>`;

    if (data.serial_number !== undefined) {
        html += `<div class="data-item"><span class="data-label">机器人序号:</span><span class="data-value">#${data.serial_number}</span></div>`;
    }
    if (data.robot_name) {
        html += `<div class="data-item"><span class="data-label">机器人名称:</span><span class="data-value">${data.robot_name}</span></div>`;
    }
    html += `<div class="data-item"><span class="data-label">运行状态:</span><span class="data-value">${data.status === 'running' ? '运行中' : '已停止'}</span></div>`;

    // MQTT相关信息
    if (lastData.publish_topic) {
        html += `<div class="data-item"><span class="data-label">发布主题:</span><span class="data-value">${lastData.publish_topic}</span></div>`;
    }
    if (lastData.subscribe_topic) {
        html += `<div class="data-item"><span class="data-label">订阅主题:</span><span class="data-value">${lastData.subscribe_topic}</span></div>`;
    }
    if (lastData.sequence !== undefined) {
        html += `<div class="data-item"><span class="data-label">MQTT消息序号:</span><span class="data-value">${lastData.sequence}</span></div>`;
    }
    if (lastData.report_interval_seconds !== undefined) {
        html += `<div class="data-item"><span class="data-label">上报间隔(秒):</span><span class="data-value">${lastData.report_interval_seconds}</span></div>`;
    }
    html += '<hr style="margin: 20px 0;">';
    html += '<h3 style="margin-bottom: 15px;">数据</h3>';

    const robotData = lastData.data || lastData;
    for (const key of Object.keys(robotData)) {
        if (key === 'current_timestamp' || key === 'local_time') continue;

        const label = LABEL_MAP[key] || key;
        const value = robotData[key];
        if (value !== null && typeof value === 'object') {
            html += `<div class="data-item"><span class="data-label">${label}:</span> ${renderObject(value)}</div>`;
        } else {
            html += `<div class="data-item"><span class="data-label">${label}:</span> <span class="data-value">${formatValue(key, value)}</span></div>`;
        }
    }

    // 时间字段
    if (robotData.current_timestamp || robotData.local_time) {
        const currentTimeStr = formatTimeObject(robotData.current_timestamp);
        const localTimeStr = formatTimeObject(robotData.local_time);

        if (currentTimeStr) {
            html += `<div class="data-item"><span class="data-label">当前时间戳:</span> <span class="data-value">${currentTimeStr}</span></div>`;
        }
        if (localTimeStr) {
            html += `<div class="data-item"><span class="data-label">本地时间:</span> <span class="data-value">${localTimeStr}</span></div>`;
        }
    }

    details.innerHTML = html;
    modal.classList.add('active');

    // 渲染清扫记录表格（如果存在）
    const cleanSection = document.getElementById('cleanRecordsSection');
    const cleanRecords = robotData.clean_records || robotData.cleanRecords || null;
    if (Array.isArray(cleanRecords) && cleanRecords.length > 0) {
        let tableHtml = '<h3 style="margin: 10px 0 8px;">最近清扫记录</h3>';
        tableHtml += '<table class="clean-records-table">';
        tableHtml += '<thead><tr><th>#</th><th>日期(天)</th><th>时间</th><th>持续(分钟)</th><th>结果</th><th>耗电量</th></tr></thead>';
        tableHtml += '<tbody>';
        for (let i = 0; i < 5; i++) {
            const rec = cleanRecords[i] || null;
            if (!rec) {
                tableHtml += `<tr><td>${i + 1}</td><td colspan="5">无数据</td></tr>`;
                continue;
            }
            const day = rec.day !== undefined ? rec.day : '';
            const hour = rec.hour !== undefined ? String(rec.hour).padStart(2, '0') : '00';
            const minute = rec.minute !== undefined ? String(rec.minute).padStart(2, '0') : '00';
            const minutes = rec.minutes !== undefined ? rec.minutes : '';
            const result = rec.result !== undefined ? (typeof rec.result === 'number' ? (rec.result === 0 ? '成功' : `代码 ${rec.result}`) : rec.result) : '';
            const energy = rec.energy !== undefined ? (typeof rec.energy === 'number' ? `${rec.energy}` : rec.energy) : '';

            tableHtml += `<tr>` +
                `<td>${i + 1}</td>` +
                `<td>${day}</td>` +
                `<td>${hour}:${minute}</td>` +
                `<td>${minutes}</td>` +
                `<td>${result}</td>` +
                `<td>${energy}</td>` +
                `</tr>`;
        }
        tableHtml += '</tbody></table>';
        cleanSection.innerHTML = tableHtml;
        cleanSection.style.display = 'block';
    } else {
        cleanSection.innerHTML = '';
        cleanSection.style.display = 'none';
    }
}

// 切换表单显示/隐藏
export function toggleForm(contentId, iconId) {
    const content = document.getElementById(contentId);
    const icon = document.getElementById(iconId);

    if (content.style.display === 'none') {
        content.style.display = content.id === 'scheduleFormContent' ? 'grid' : 'block';
        icon.textContent = '▲';
    } else {
        content.style.display = 'none';
        icon.textContent = '▼';
    }
}
