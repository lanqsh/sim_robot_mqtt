// 分页管理模块
export class PaginationManager {
    constructor() {
        this.currentPage = 1;
        this.pageSize = 20;
        this.totalPages = 0;
        this.totalCount = 0;
    }

    // 更新分页信息
    updatePagination(pagination) {
        this.totalPages = pagination.totalPages;
        this.totalCount = pagination.total;
    }

    // 渲染分页控件
    renderPagination() {
        const pagination = document.getElementById('pagination');
        if (!pagination) {
            console.warn('元素 #pagination 未找到，跳过渲染分页控件');
            return;
        }

        if (this.totalPages <= 1) {
            pagination.innerHTML = '';
            return;
        }

        let html = '<div class="pagination-buttons">';

        // 左侧：页码区域
        html += '<div class="page-numbers">';

        // 页码按钮
        const maxButtons = 7;
        let startPage = Math.max(1, this.currentPage - Math.floor(maxButtons / 2));
        let endPage = Math.min(this.totalPages, startPage + maxButtons - 1);

        if (endPage - startPage < maxButtons - 1) {
            startPage = Math.max(1, endPage - maxButtons + 1);
        }

        if (startPage > 1) {
            html += `<button class="page-btn" onclick="window.goToPage(1)">1</button>`;
            if (startPage > 2) {
                html += '<span class="page-ellipsis">...</span>';
            }
        }

        for (let i = startPage; i <= endPage; i++) {
            if (i === this.currentPage) {
                html += `<button class="page-btn active">${i}</button>`;
            } else {
                html += `<button class="page-btn" onclick="window.goToPage(${i})">${i}</button>`;
            }
        }

        if (endPage < this.totalPages) {
            if (endPage < this.totalPages - 1) {
                html += '<span class="page-ellipsis">...</span>';
            }
            html += `<button class="page-btn" onclick="window.goToPage(${this.totalPages})">${this.totalPages}</button>`;
        }

        html += '</div>';

        // 中间：页面大小和总数信息
        html += '<div class="page-info">';
        html += '<label for="pageSize">每页显示：</label>';
        html += '<select id="pageSize" onchange="window.changePageSize()">';
        html += '<option value="10"' + (this.pageSize === 10 ? ' selected' : '') + '>10</option>';
        html += '<option value="20"' + (this.pageSize === 20 ? ' selected' : '') + '>20</option>';
        html += '<option value="50"' + (this.pageSize === 50 ? ' selected' : '') + '>50</option>';
        html += '<option value="100"' + (this.pageSize === 100 ? ' selected' : '') + '>100</option>';
        html += '</select>';
        html += `<span class="total-info">共 ${this.totalCount} 个机器人</span>`;
        html += '</div>';

        // 右侧：上一页/下一页按钮
        html += '<div class="page-nav-buttons">';

        // 上一页按钮
        if (this.currentPage > 1) {
            html += `<button class="page-nav-btn" onclick="window.goToPage(${this.currentPage - 1})">上一页</button>`;
        } else {
            html += '<button class="page-nav-btn" disabled>上一页</button>';
        }

        // 下一页按钮
        if (this.currentPage < this.totalPages) {
            html += `<button class="page-nav-btn" onclick="window.goToPage(${this.currentPage + 1})">下一页</button>`;
        } else {
            html += '<button class="page-nav-btn" disabled>下一页</button>';
        }

        html += '</div>';

        html += '</div>';
        pagination.innerHTML = html;
    }

    // 跳转到指定页
    goToPage(page) {
        this.currentPage = page;
        return this.currentPage;
    }

    // 改变每页显示数量
    changePageSize(newPageSize) {
        this.pageSize = newPageSize;
        this.currentPage = 1;
    }

    // 获取当前页码
    getCurrentPage() {
        return this.currentPage;
    }

    // 获取每页大小
    getPageSize() {
        return this.pageSize;
    }

    // 获取总数
    getTotalCount() {
        return this.totalCount;
    }
}
