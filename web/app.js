const API_BASE = window.location.origin;

// 分页状态
let currentPage = 1;
let pageSize = 20;
let totalPages = 0;
let totalCount = 0;

// 加载机器人（从后端分页）
async function loadRobots() {
    try {
        const response = await fetch(`${API_BASE}/api/robots?page=${currentPage}&pageSize=${pageSize}`);
        const result = await response.json();

        const container = document.getElementById('robotsList');

        // 更新分页和统计信息
        totalPages = result.pagination.totalPages;
        totalCount = result.pagination.total;

        if (totalCount === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <h3>暂无机器人</h3>
                    <p>请添加新机器人开始管理</p>
                </div>
            `;
            document.getElementById('pagination').innerHTML = '';
            document.getElementById('totalCount').textContent = '';
            updateStatistics(result.statistics);
            return;
        }

        // 显示总数
        document.getElementById('totalCount').textContent = `共 ${totalCount} 个机器人`;

        // 更新统计信息
        updateStatistics(result.statistics);

        // 渲染当前页
        renderPage(result.data);
    } catch (error) {
        console.error('加载机器人失败:', error);
        document.getElementById('robotsList').innerHTML = `
            <div class="error-message">加载机器人列表失败: ${error.message}</div>
        `;
    }
}

// 更新统计信息
function updateStatistics(statistics) {
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

// 渲染当前页的机器人
function renderPage(robots) {
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
                            onclick="toggleRobotStatus('${robot.robot_id}', ${robot.enabled})">
                        ${robot.enabled ? '禁用' : '启用'}
                    </button>
                    <button class="btn btn-primary" onclick="viewRobotData('${robot.robot_id}')">
                        查看数据
                    </button>
                    <button class="btn btn-danger" onclick="deleteRobot('${robot.robot_id}')">
                        删除
                    </button>
                </div>
            `;
            grid.appendChild(card);
        });

    // 渲染分页控件
    renderPagination();
}

// 渲染分页控件
function renderPagination() {
    const pagination = document.getElementById('pagination');

    if (totalPages <= 1) {
        pagination.innerHTML = '';
        return;
    }

    let html = '<div class="pagination-buttons">';

    // 上一页按钮
    if (currentPage > 1) {
        html += `<button class="page-btn" onclick="goToPage(${currentPage - 1})">上一页</button>`;
    } else {
        html += '<button class="page-btn" disabled>上一页</button>';
    }

    // 页码按钮
    const maxButtons = 7;
    let startPage = Math.max(1, currentPage - Math.floor(maxButtons / 2));
    let endPage = Math.min(totalPages, startPage + maxButtons - 1);

    if (endPage - startPage < maxButtons - 1) {
        startPage = Math.max(1, endPage - maxButtons + 1);
    }

    if (startPage > 1) {
        html += `<button class="page-btn" onclick="goToPage(1)">1</button>`;
        if (startPage > 2) {
            html += '<span class="page-ellipsis">...</span>';
        }
    }

    for (let i = startPage; i <= endPage; i++) {
        if (i === currentPage) {
            html += `<button class="page-btn active">${i}</button>`;
        } else {
            html += `<button class="page-btn" onclick="goToPage(${i})">${i}</button>`;
        }
    }

    if (endPage < totalPages) {
        if (endPage < totalPages - 1) {
            html += '<span class="page-ellipsis">...</span>';
        }
        html += `<button class="page-btn" onclick="goToPage(${totalPages})">${totalPages}</button>`;
    }

    // 下一页按钮
    if (currentPage < totalPages) {
        html += `<button class="page-btn" onclick="goToPage(${currentPage + 1})">下一页</button>`;
    } else {
        html += '<button class="page-btn" disabled>下一页</button>';
    }

    html += '</div>';
    pagination.innerHTML = html;
}

// 跳转到指定页
function goToPage(page) {
    currentPage = page;
    loadRobots();
    // 滚动到顶部
    document.querySelector('.robots-container').scrollIntoView({ behavior: 'smooth' });
}

// 改变每页显示数量
function changePageSize() {
    pageSize = parseInt(document.getElementById('pageSize').value);
    currentPage = 1;
    loadRobots();
}

// 切换机器人启用状态
async function toggleRobotStatus(robotId, currentStatus) {
    const newStatus = !currentStatus;
    const action = newStatus ? '启用' : '禁用';

    if (!confirm(`确定要${action}该机器人吗？`)) {
        return;
    }

    try {
        const response = await fetch(`${API_BASE}/api/robots/${robotId}/status`, {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                enabled: newStatus
            })
        });

        const result = await response.json();

        if (result.success) {
            alert(result.message);
            loadRobots(); // 重新加载列表
        } else {
            alert('操作失败: ' + result.error);
        }
    } catch (error) {
        console.error('切换状态失败:', error);
        alert('切换状态失败: ' + error.message);
    }
}

// 添加机器人
document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('addRobotForm').addEventListener('submit', async (e) => {
        e.preventDefault();

        const robotName = document.getElementById('robotName').value.trim();
        const serialNumber = parseInt(document.getElementById('serialNumber').value);

        if (!serialNumber || serialNumber < 1) {
            alert('序号必须填写且大于0');
            return;
        }

        try {
            const response = await fetch(`${API_BASE}/api/robots`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    robot_name: robotName || `Robot ${serialNumber}`,
                    serial_number: serialNumber
                })
            });

            const result = await response.json();

            if (result.success) {
                alert('机器人添加成功!');
                document.getElementById('addRobotForm').reset();
                loadRobots();
            } else {
                alert(`添加失败: ${result.error}`);
            }
        } catch (error) {
            console.error('添加机器人失败:', error);
            alert(`添加失败: ${error.message}`);
        }
    });

    // 页面加载时获取机器人列表
    loadRobots();

    // 每10秒刷新一次列表
    setInterval(loadRobots, 10000);

    // 点击模态框背景关闭
    document.getElementById('robotModal').addEventListener('click', (e) => {
        if (e.target.id === 'robotModal') {
            closeModal();
        }
    });
});

// 删除机器人
async function deleteRobot(robotId) {
    if (!confirm(`确定要删除机器人 ${robotId} 吗？`)) {
        return;
    }

    try {
        const response = await fetch(`${API_BASE}/api/robots/${robotId}`, {
            method: 'DELETE'
        });

        const result = await response.json();

        if (result.success) {
            alert('机器人删除成功!');
            loadRobots();
        } else {
            alert(`删除失败: ${result.error}`);
        }
    } catch (error) {
        console.error('删除机器人失败:', error);
        alert(`删除失败: ${error.message}`);
    }
}

// 查看机器人数据
async function viewRobotData(robotId) {
    const modal = document.getElementById('robotModal');
    const details = document.getElementById('robotDetails');

    modal.classList.add('active');
    details.innerHTML = '<div class="loading">加载中...</div>';

    try {
        const response = await fetch(`${API_BASE}/api/robots/${robotId}/data`);
        const data = await response.json();

        if (data.robot_id) {
            const lastData = typeof data.last_data === 'string'
                ? JSON.parse(data.last_data)
                : data.last_data;

            // 字段中文映射
            const labelMap = {
                robot_id: '机器人ID',
                publish_topic: '发布主题',
                subscribe_topic: '订阅主题',
                sequence: 'MQTT消息序号',
                report_interval_seconds: '上报间隔(秒)',
                running: '运行状态',
                // 顶层/基本
                data: '数据',

                // RobotData
                main_motor_current: '主电机电流',
                slave_motor_current: '从电机电流',
                battery_voltage: '电池电压',
                battery_current: '电池电流',
                battery_status: '电池状态',
                battery_level: '电池电量',
                battery_temperature: '电池温度',
                position_info: '位置信息',
                working_duration: '工作时长',
                total_run_count: '累计运行次数',
                current_lap_count: '当前圈数',
                solar_voltage: '光伏电压',
                solar_current: '光伏电流',
                board_temperature: '主板温度',
                robot_number: '机器人编号',
                software_version: '软件版本',
                parking_position: '停机位',
                daytime_scan_protect: '白天防误扫',
                schedule_tasks: '定时任务',
                enabled: '是否启用',
                motor_params: '电机参数',
                temp_voltage_protection: '温压保护参数',
                local_time: '本地时间',
                environment_info: '环境信息',
                master_currents: '主机电流',
                slave_currents: '从机电流',
                position: '位置',
                direction: '方向',
                module_eui: '模组EUI',
                domestic_foreign_flag: '国内/国外版本',
                country_code: '国家代码',
                region_code: '地区代码',
                project_code: '项目代码',

                // lora_params 内部
                lora_params: 'LoRa参数',
                power: '功率',
                frequency: '频率',
                rate: '速率',

                // local_time 内部
                year: '年',
                month: '月',
                day: '日',
                second: '秒',

                // environment_info 内部
                sensor_temperature: '传感器温度',
                sensor_humidity: '传感器湿度',
                ambient_temperature: '环境温度',
                day_night_status: '白夜状态',

                // current_timestamp
                current_timestamp: '当前时间戳',

                // motor_params 详细
                walk_motor_speed: '行走电机速率',
                brush_motor_speed: '毛刷电机速率',
                windproof_motor_speed: '防风电机速率',
                walk_motor_max_current_ma: '行走电机上限电流停机值(mA)',
                brush_motor_max_current_ma: '毛刷电机上限电流停机值(mA)',
                windproof_motor_max_current_ma: '防风电机上限电流停机值(mA)',
                walk_motor_warning_current_ma: '行走电机预警电流(mA)',
                brush_motor_warning_current_ma: '毛刷电机预警电流(mA)',
                windproof_motor_warning_current_ma: '防风电机预警电流(mA)',
                walk_motor_mileage_m: '行走电机运行里程(m)',
                brush_motor_timeout_s: '毛刷电机超时(s)',
                windproof_motor_timeout_s: '防风电机超时(s)',
                reverse_time_s: '反转时间(s)',
                protection_angle: '保护角度',

                // temp_voltage_protection 详细
                protection_current_ma: '保护电流(mA)',
                high_temp_threshold: '高温阈值',
                low_temp_threshold: '低温阈值',
                protection_temp: '保护温度',
                recovery_temp: '恢复温度',
                protection_voltage: '保护电压',
                recovery_voltage: '恢复电压',
                protection_battery_level: '保护电量',
                limit_run_battery_level: '限制运行电量',
                recovery_battery_level: '恢复电量',
                board_protection_temp: '主板保护温度',
                board_recovery_temp: '主板恢复温度',

                // schedule task fields
                weekday: '星期',
                hour: '小时',
                minute: '分钟',
                run_count: '运行次数',

                // 其他常用
                robot_name: '机器人名称',
                serial_number: '序号'
            };

            // 简单字段格式化
            function formatValue(key, val) {
                if (val === null) return 'null';
                if (key === 'battery_level') return `${val}%`;
                if (key === 'battery_voltage' || key === 'solar_voltage') {
                    // 后端以 100mV 为单位，前端显示 V（保留1位）
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

            // 递归渲染对象为HTML（使用labelMap映射）
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
                    const label = labelMap[key] || key;
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

            let html = '';
            html += `<div class="data-item"><span class="data-label">机器人ID:</span><span class="data-value">${data.robot_id}</span></div>`;
            if (data.serial_number !== undefined) {
                html += `<div class="data-item"><span class="data-label">机器人序号:</span><span class="data-value">#${data.serial_number}</span></div>`;
            }
            if (data.robot_name) {
                html += `<div class="data-item"><span class="data-label">机器人名称:</span><span class="data-value">${data.robot_name}</span></div>`;
            }
            html += `<div class="data-item"><span class="data-label">运行状态:</span><span class="data-value">${data.status === 'running' ? '运行中' : '已停止'}</span></div>`;
            html += '<hr style="margin: 20px 0;">';
            html += '<h3 style="margin-bottom: 15px;">完整成员变量</h3>';

            // lastData 可能包含 top-level 字段和 data 字段
            html += renderObject(lastData);

            details.innerHTML = html;
        } else {
            details.innerHTML = `<div class="error-message">${data.error || '无法获取机器人数据'}</div>`;
        }
    } catch (error) {
        console.error('获取机器人数据失败:', error);
        details.innerHTML = `<div class="error-message">获取数据失败: ${error.message}</div>`;
    }
}

// 批量添加机器人
async function batchAddRobots() {
    const startSerial = parseInt(document.getElementById('batchStartSerial').value);
    const endSerial = parseInt(document.getElementById('batchEndSerial').value);
    const namePrefix = document.getElementById('batchNamePrefix').value.trim() || 'Robot ';

    if (!startSerial || !endSerial) {
        alert('请填写起始序号和结束序号');
        return;
    }

    if (startSerial > endSerial) {
        alert('起始序号不能大于结束序号');
        return;
    }

    const count = endSerial - startSerial + 1;
    if (!confirm(`确定要批量添加 ${count} 个机器人吗？`)) {
        return;
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
        showLoading(`正在添加 ${count} 个机器人...`);
        const response = await fetch(`${API_BASE}/api/robots/batch`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ robots })
        });

        const result = await response.json();
        hideLoading();

        if (result.success) {
            alert(`批量添加成功！共添加 ${result.count} 个机器人`);
            loadRobots();
            // 清空输入框
            document.getElementById('batchStartSerial').value = '';
            document.getElementById('batchEndSerial').value = '';
            document.getElementById('batchNamePrefix').value = '';
        } else {
            alert('批量添加失败: ' + result.error);
        }
    } catch (error) {
        hideLoading();
        console.error('批量添加机器人失败:', error);
        alert('批量添加失败: ' + error.message);
    }
}

// 显示加载提示
function showLoading(text = '处理中...') {
    document.getElementById('loadingText').textContent = text;
    document.getElementById('loadingModal').classList.add('active');
}

// 隐藏加载提示
function hideLoading() {
    document.getElementById('loadingModal').classList.remove('active');
}

// 批量删除机器人
async function batchDeleteRobots() {
    const startSerial = parseInt(document.getElementById('batchStartSerial').value);
    const endSerial = parseInt(document.getElementById('batchEndSerial').value);

    if (!startSerial || !endSerial) {
        alert('请填写起始序号和结束序号');
        return;
    }

    if (startSerial > endSerial) {
        alert('起始序号不能大于结束序号');
        return;
    }

    const count = endSerial - startSerial + 1;
    if (!confirm(`确定要批量删除 ${count} 个机器人吗？此操作不可恢复！`)) {
        return;
    }

    let robot_ids = [];

    // 从服务器获取指定序号范围的机器人（仅使用后端分页接口）
    try {
        showLoading('正在获取机器人列表...');
        let robots = [];

        // 按页拉取所有数据，使用当前前端 pageSize 设置
        let page = 1;
        while (true) {
            const resp = await fetch(`${API_BASE}/api/robots?page=${page}&pageSize=${pageSize}`);
            if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
            const json = await resp.json();

            const pageData = json.data || [];
            robots = robots.concat(pageData);

            const pagination = json.pagination;
            if (!pagination) break; // 如果没有分页信息则停止
            if (page >= pagination.totalPages) break;
            page += 1;
        }

        // 过滤出序号在范围内的机器人
        robot_ids = robots
            .filter(robot => robot.serial_number >= startSerial && robot.serial_number <= endSerial)
            .map(robot => robot.robot_id);

        if (robot_ids.length === 0) {
            hideLoading();
            alert('在指定序号范围内没有找到机器人');
            return;
        }
    } catch (error) {
        hideLoading();
        console.error('获取机器人列表失败:', error);
        alert('获取机器人列表失败: ' + error.message);
        return;
    }

    try {
        showLoading(`正在删除 ${robot_ids.length} 个机器人...`);
        const response = await fetch(`${API_BASE}/api/robots/batch-delete`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ robot_ids })
        });

        const result = await response.json();
        hideLoading();

        if (result.success) {
            alert(`批量删除成功！共删除 ${result.count} 个机器人`);
            loadRobots();
        } else {
            alert('批量删除失败: ' + result.error);
        }
    } catch (error) {
        hideLoading();
        console.error('批量删除机器人失败:', error);
        alert('批量删除失败: ' + error.message);
    }
}

// 关闭模态框
function closeModal() {
    document.getElementById('robotModal').classList.remove('active');
}
