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
                    html += `<div class="data-item"><span class="data-label">${key}:</span> ${renderObject(obj[key])}</div>`;
                }
                html += '</div>';
                return html;
            }

            let html = '';
            html += `<div class="data-item"><span class="data-label">机器人ID:</span><span class="data-value">${data.robot_id}</span></div>`;
            html += `<div class="data-item"><span class="data-label">运行状态:</span><span class="data-value">${data.status}</span></div>`;
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
        const response = await fetch(`${API_BASE}/api/robots/batch`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ robots })
        });

        const result = await response.json();

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
        console.error('批量添加机器人失败:', error);
        alert('批量添加失败: ' + error.message);
    }
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

    // 从服务器获取指定序号范围的机器人
    try {
        const response = await fetch(`${API_BASE}/api/robots`);
        const robots = await response.json();

        // 过滤出序号在范围内的机器人
        robot_ids = robots
            .filter(robot => robot.serial_number >= startSerial && robot.serial_number <= endSerial)
            .map(robot => robot.robot_id);

        if (robot_ids.length === 0) {
            alert('在指定序号范围内没有找到机器人');
            return;
        }
    } catch (error) {
        console.error('获取机器人列表失败:', error);
        alert('获取机器人列表失败: ' + error.message);
        return;
    }

    try {
        const response = await fetch(`${API_BASE}/api/robots/batch-delete`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ robot_ids })
        });

        const result = await response.json();

        if (result.success) {
            alert(`批量删除成功！共删除 ${result.count} 个机器人`);
            loadRobots();
        } else {
            alert('批量删除失败: ' + result.error);
        }
    } catch (error) {
        console.error('批量删除机器人失败:', error);
        alert('批量删除失败: ' + error.message);
    }
}

// 关闭模态框
function closeModal() {
    document.getElementById('robotModal').classList.remove('active');
}
