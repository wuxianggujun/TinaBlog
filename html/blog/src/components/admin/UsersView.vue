<template>
  <div class="users-view">
    <div class="section-header">
      <h2>用户管理</h2>
      <p>管理系统用户，包括封禁和删除操作</p>
    </div>
    
    <!-- 搜索和筛选 -->
    <div class="filter-bar">
      <div class="search-box">
        <input 
          type="text" 
          v-model="searchTerm" 
          placeholder="搜索用户名或邮箱..." 
          @input="debounceSearch"
        >
        <span class="search-icon">🔍</span>
      </div>
      
      <div class="filter-options">
        <select v-model="filterStatus">
          <option value="all">所有用户</option>
          <option value="active">正常用户</option>
          <option value="banned">已封禁用户</option>
          <option value="admin">管理员</option>
        </select>
      </div>
    </div>
    
    <!-- 用户列表 -->
    <div class="users-list" v-if="filteredUsers.length > 0">
      <table>
        <thead>
          <tr>
            <th>用户头像</th>
            <th>用户名</th>
            <th>显示名称</th>
            <th>Email</th>
            <th>注册时间</th>
            <th>状态</th>
            <th>操作</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="user in paginatedUsers" :key="user.uuid" :class="{ 'banned': user.is_banned, 'admin': user.is_admin }">
            <td>
              <div class="user-avatar">
                <img v-if="user.avatar" :src="user.avatar" :alt="user.display_name || user.username">
                <div v-else class="avatar-placeholder">{{ getInitials(user.display_name || user.username) }}</div>
              </div>
            </td>
            <td>{{ user.username }}</td>
            <td>{{ user.display_name || '-' }}</td>
            <td>{{ user.email }}</td>
            <td>{{ formatDate(user.created_at) }}</td>
            <td>
              <span class="status-badge" :class="getUserStatusClass(user)">
                {{ getUserStatusText(user) }}
              </span>
            </td>
            <td class="actions">
              <div class="action-buttons">
                <button 
                  v-if="user.is_banned" 
                  class="unban-btn" 
                  @click="confirmToggleBanStatus(user)"
                  :disabled="isCurrentUser(user) || user.is_admin"
                >
                  解除封禁
                </button>
                <button 
                  v-else 
                  class="ban-btn" 
                  @click="confirmToggleBanStatus(user)"
                  :disabled="isCurrentUser(user) || user.is_admin"
                >
                  封禁用户
                </button>
                <button 
                  class="delete-btn" 
                  @click="confirmDeleteUser(user)"
                  :disabled="isCurrentUser(user) || user.is_admin"
                >
                  删除
                </button>
              </div>
            </td>
          </tr>
        </tbody>
      </table>
    </div>
    
    <div v-else class="empty-state">
      <div class="empty-icon">🔍</div>
      <p>未找到符合条件的用户</p>
      <button @click="resetFilters">重置筛选条件</button>
    </div>
    
    <!-- 分页控制 -->
    <div class="pagination" v-if="totalPages > 1">
      <button 
        class="page-btn" 
        :class="{ disabled: currentPage === 1 }"
        @click="currentPage > 1 && (currentPage--)"
      >
        ← 上一页
      </button>
      
      <div class="page-numbers">
        <template v-for="page in paginationRange" :key="page">
          <button 
            class="page-number" 
            :class="{ active: page === currentPage }"
            @click="currentPage = page"
          >
            {{ page }}
          </button>
        </template>
      </div>
      
      <button 
        class="page-btn"
        :class="{ disabled: currentPage === totalPages }"
        @click="currentPage < totalPages && (currentPage++)"
      >
        下一页 →
      </button>
    </div>
    
    <!-- 封禁确认弹窗 -->
    <div class="modal" v-if="showBanModal">
      <div class="modal-content">
        <div class="modal-header">
          <h3>{{ selectedUser && selectedUser.is_banned ? '解除封禁确认' : '封禁用户确认' }}</h3>
          <button class="close-btn" @click="showBanModal = false">×</button>
        </div>
        <div class="modal-body">
          <p v-if="selectedUser && selectedUser.is_banned">
            您确定要解除用户 <strong>{{ selectedUser.display_name || selectedUser.username }}</strong> 的封禁状态吗？
            解除封禁后，该用户将能够重新登录和使用系统。
          </p>
          <p v-else-if="selectedUser">
            您确定要封禁用户 <strong>{{ selectedUser.display_name || selectedUser.username }}</strong> 吗？
            封禁后，该用户将无法登录系统。
          </p>
          
          <div class="form-group" v-if="selectedUser && !selectedUser.is_banned">
            <label for="banReason">封禁原因：</label>
            <textarea 
              id="banReason" 
              v-model="banReason" 
              placeholder="输入封禁原因，将通知用户..."
              rows="3"
            ></textarea>
          </div>
        </div>
        <div class="modal-footer">
          <button class="cancel-btn" @click="showBanModal = false">取消</button>
          <button 
            class="confirm-btn" 
            :class="selectedUser && selectedUser.is_banned ? 'unban' : 'ban'"
            @click="toggleBanStatus"
          >
            {{ selectedUser && selectedUser.is_banned ? '确认解除封禁' : '确认封禁' }}
          </button>
        </div>
      </div>
    </div>
    
    <!-- 删除确认弹窗 -->
    <div class="modal" v-if="showDeleteModal">
      <div class="modal-content">
        <div class="modal-header">
          <h3>删除用户确认</h3>
          <button class="close-btn" @click="showDeleteModal = false">×</button>
        </div>
        <div class="modal-body">
          <p v-if="selectedUser">
            您确定要删除用户 <strong>{{ selectedUser.display_name || selectedUser.username }}</strong> 吗？
            此操作将永久删除该用户及其所有数据，且无法恢复！
          </p>
          
          <div class="warning-box">
            <p>⚠️ 警告：删除用户将同时删除该用户的以下内容：</p>
            <ul>
              <li>所有文章</li>
              <li>所有评论</li>
              <li>所有个人资料信息</li>
            </ul>
          </div>
          
          <div class="form-group">
            <label for="confirmDelete">请输入 "DELETE" 确认删除：</label>
            <input 
              type="text" 
              id="confirmDelete" 
              v-model="deleteConfirmation" 
              placeholder='输入 "DELETE" 确认'
            >
          </div>
        </div>
        <div class="modal-footer">
          <button class="cancel-btn" @click="showDeleteModal = false">取消</button>
          <button 
            class="confirm-btn delete" 
            @click="deleteUser"
            :disabled="deleteConfirmation !== 'DELETE'"
          >
            确认删除
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';
  
export default {
  name: 'UsersView',
  props: {
    users: {
      type: Array,
      default: () => []
    }
  },
  data() {
    return {
      searchTerm: '',
      filterStatus: 'all',
      currentPage: 1,
      itemsPerPage: 10,
      showBanModal: false,
      showDeleteModal: false,
      selectedUser: null,
      banReason: '',
      deleteConfirmation: '',
      isLoading: false,
      currentUserInfo: null,
      localUsers: []
    };
  },
  computed: {
    filteredUsers() {
      let result = this.localUsers;
      
      // 搜索过滤
      if (this.searchTerm) {
        const term = this.searchTerm.toLowerCase();
        result = result.filter(user => 
          user.username.toLowerCase().includes(term) ||
          (user.display_name && user.display_name.toLowerCase().includes(term)) ||
          user.email.toLowerCase().includes(term)
        );
      }
      
      // 状态过滤
      if (this.filterStatus !== 'all') {
        switch (this.filterStatus) {
          case 'active':
            result = result.filter(user => !user.is_banned && !user.is_admin);
            break;
          case 'banned':
            result = result.filter(user => user.is_banned);
            break;
          case 'admin':
            result = result.filter(user => user.is_admin);
            break;
        }
      }
      
      return result;
    },
    totalPages() {
      return Math.ceil(this.filteredUsers.length / this.itemsPerPage);
    },
    paginatedUsers() {
      const start = (this.currentPage - 1) * this.itemsPerPage;
      const end = start + this.itemsPerPage;
      return this.filteredUsers.slice(start, end);
    },
    paginationRange() {
      const range = [];
      const maxVisiblePages = 5;
      
      if (this.totalPages <= maxVisiblePages) {
        // 如果总页数小于或等于最大可见页数，显示所有页码
        for (let i = 1; i <= this.totalPages; i++) {
          range.push(i);
        }
      } else {
        // 确定显示哪些页码
        let start = Math.max(1, this.currentPage - 2);
        let end = Math.min(this.totalPages, start + maxVisiblePages - 1);
        
        // 调整开始位置以确保我们显示完整的maxVisiblePages页
        if (end === this.totalPages) {
          start = Math.max(1, end - maxVisiblePages + 1);
        }
        
        for (let i = start; i <= end; i++) {
          range.push(i);
        }
      }
      
      return range;
    }
  },
  watch: {
    users: {
      immediate: true,
      handler(newVal) {
        if (newVal && newVal.length) {
          this.localUsers = [...newVal];
        }
      }
    },
    filterStatus() {
      this.currentPage = 1; // 重置到第一页
    },
    searchTerm() {
      this.currentPage = 1; // 重置到第一页
    }
  },
  created() {
    this.fetchUsers();
    this.fetchCurrentUserInfo();
  },
  methods: {
    async fetchUsers() {
      this.isLoading = true;
      try {
        const response = await axios.get('/api/admin/users');
        if (response.data.code === 0) {
          this.localUsers = response.data.data || [];
        } else {
          console.error('获取用户列表失败:', response.data.message);
        }
      } catch (error) {
        console.error('获取用户列表请求错误:', error);
      } finally {
        this.isLoading = false;
      }
    },
    
    async fetchCurrentUserInfo() {
      try {
        const response = await axios.get('/api/user/info');
        if (response.data.code === 0) {
          this.currentUserInfo = response.data.data;
        }
      } catch (error) {
        console.error('获取当前用户信息失败:', error);
      }
    },
    
    isCurrentUser(user) {
      return this.currentUserInfo && this.currentUserInfo.uuid === user.uuid;
    },
    
    getInitials(name) {
      if (!name) return '';
      return name.charAt(0).toUpperCase();
    },
    
    formatDate(dateString) {
      if (!dateString) return '-';
      const date = new Date(dateString);
      return date.toLocaleDateString('zh-CN', {
        year: 'numeric', 
        month: '2-digit', 
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit'
      });
    },
    
    getUserStatusClass(user) {
      if (user.is_admin) return 'admin';
      if (user.is_banned) return 'banned';
      return 'active';
    },
    
    getUserStatusText(user) {
      if (user.is_admin) return '管理员';
      if (user.is_banned) return '已封禁';
      return '正常';
    },
    
    confirmToggleBanStatus(user) {
      this.selectedUser = user;
      this.banReason = '';
      this.showBanModal = true;
    },
    
    async toggleBanStatus() {
      if (!this.selectedUser) return;
      
      this.isLoading = true;
      try {
        const action = this.selectedUser.is_banned ? 'unban' : 'ban';
        const response = await axios.post(`/api/admin/users/${this.selectedUser.uuid}/${action}`, {
          reason: this.banReason
        });
        
        if (response.data.code === 0) {
          // 更新本地用户数据
          const index = this.localUsers.findIndex(u => u.uuid === this.selectedUser.uuid);
          if (index !== -1) {
            const updatedUser = { ...this.localUsers[index] };
            updatedUser.is_banned = !updatedUser.is_banned;
            this.$set(this.localUsers, index, updatedUser);
          }
          
          // 关闭弹窗并重置状态
          this.showBanModal = false;
          this.selectedUser = null;
          this.banReason = '';
          
          // 通知父组件重新加载数据
          this.$emit('reload');
        } else {
          console.error(`${action === 'ban' ? '封禁' : '解除封禁'}用户失败:`, response.data.message);
          alert(`操作失败: ${response.data.message}`);
        }
      } catch (error) {
        console.error('用户状态更新请求错误:', error);
        alert('操作失败，请稍后重试');
      } finally {
        this.isLoading = false;
      }
    },
    
    confirmDeleteUser(user) {
      this.selectedUser = user;
      this.deleteConfirmation = '';
      this.showDeleteModal = true;
    },
    
    async deleteUser() {
      if (!this.selectedUser || this.deleteConfirmation !== 'DELETE') return;
      
      this.isLoading = true;
      try {
        const response = await axios.delete(`/api/admin/users/${this.selectedUser.uuid}`);
        
        if (response.data.code === 0) {
          // 更新本地用户数据
          this.localUsers = this.localUsers.filter(u => u.uuid !== this.selectedUser.uuid);
          
          // 关闭弹窗并重置状态
          this.showDeleteModal = false;
          this.selectedUser = null;
          this.deleteConfirmation = '';
          
          // 通知父组件重新加载数据
          this.$emit('reload');
        } else {
          console.error('删除用户失败:', response.data.message);
          alert(`删除失败: ${response.data.message}`);
        }
      } catch (error) {
        console.error('删除用户请求错误:', error);
        alert('删除失败，请稍后重试');
      } finally {
        this.isLoading = false;
      }
    },
    
    resetFilters() {
      this.searchTerm = '';
      this.filterStatus = 'all';
      this.currentPage = 1;
    },
    
    debounceSearch: function() {
      clearTimeout(this._searchTimeout);
      this._searchTimeout = setTimeout(() => {
        // 搜索已经通过计算属性实现
      }, 300);
    }
  }
};
</script>

<style scoped>
.users-view {
  background-color: #fff;
  border-radius: 8px;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
  padding: 24px;
  margin-bottom: 24px;
}

.section-header {
  margin-bottom: 24px;
}

.section-header h2 {
  font-size: 1.5rem;
  color: #2c3e50;
  margin-bottom: 8px;
}

.section-header p {
  color: #7f8c8d;
  font-size: 0.9rem;
}

/* 筛选栏 */
.filter-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  flex-wrap: wrap;
  gap: 16px;
}

.search-box {
  position: relative;
  flex: 1;
  min-width: 250px;
}

.search-box input {
  width: 100%;
  padding: 10px 12px 10px 36px;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 0.9rem;
}

.search-icon {
  position: absolute;
  left: 12px;
  top: 50%;
  transform: translateY(-50%);
  color: #95a5a6;
}

.filter-options select {
  padding: 10px 16px;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 0.9rem;
  background-color: white;
  min-width: 150px;
}

/* 用户列表表格 */
.users-list {
  overflow-x: auto;
  margin-bottom: 20px;
}

table {
  width: 100%;
  border-collapse: collapse;
}

thead th {
  background-color: #f5f7fa;
  color: #34495e;
  padding: 12px 16px;
  text-align: left;
  font-weight: 600;
  font-size: 0.9rem;
  border-bottom: 2px solid #eee;
}

tbody td {
  padding: 12px 16px;
  border-bottom: 1px solid #eee;
  color: #2c3e50;
  font-size: 0.9rem;
}

tr.banned {
  background-color: #ffefef;
}

tr.admin {
  background-color: #effaff;
}

.user-avatar {
  width: 36px;
  height: 36px;
  border-radius: 50%;
  overflow: hidden;
}

.user-avatar img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.avatar-placeholder {
  width: 100%;
  height: 100%;
  background-color: #3498db;
  color: white;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: bold;
}

/* 状态标签 */
.status-badge {
  display: inline-block;
  padding: 4px 8px;
  border-radius: 12px;
  font-size: 0.8rem;
  font-weight: 500;
}

.status-badge.active {
  background-color: #e1f5e1;
  color: #27ae60;
}

.status-badge.banned {
  background-color: #ffecec;
  color: #e74c3c;
}

.status-badge.admin {
  background-color: #e1f0ff;
  color: #3498db;
}

/* 操作按钮 */
.actions {
  white-space: nowrap;
}

.action-buttons {
  display: flex;
  gap: 8px;
}

.action-buttons button {
  padding: 6px 12px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.8rem;
  font-weight: 500;
  transition: background-color 0.2s;
}

.action-buttons button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.ban-btn {
  background-color: #f39c12;
  color: white;
}

.ban-btn:hover:not(:disabled) {
  background-color: #d35400;
}

.unban-btn {
  background-color: #2ecc71;
  color: white;
}

.unban-btn:hover:not(:disabled) {
  background-color: #27ae60;
}

.delete-btn {
  background-color: #e74c3c;
  color: white;
}

.delete-btn:hover:not(:disabled) {
  background-color: #c0392b;
}

/* 空状态 */
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 48px 0;
  text-align: center;
}

.empty-icon {
  font-size: 3rem;
  margin-bottom: 16px;
  color: #bdc3c7;
}

.empty-state p {
  color: #7f8c8d;
  margin-bottom: 16px;
}

.empty-state button {
  padding: 8px 16px;
  background-color: #3498db;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.empty-state button:hover {
  background-color: #2980b9;
}

/* 分页 */
.pagination {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-top: 24px;
}

.page-btn {
  padding: 6px 12px;
  border: 1px solid #ddd;
  background-color: white;
  cursor: pointer;
  transition: all 0.2s;
}

.page-btn:hover:not(.disabled) {
  background-color: #f5f7fa;
}

.page-btn.disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.page-numbers {
  display: flex;
  margin: 0 8px;
}

.page-number {
  width: 36px;
  height: 36px;
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid #ddd;
  margin: 0 4px;
  cursor: pointer;
  transition: all 0.2s;
}

.page-number:hover:not(.active) {
  background-color: #f5f7fa;
}

.page-number.active {
  background-color: #3498db;
  color: white;
  border-color: #3498db;
}

/* 弹窗 */
.modal {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: rgba(0, 0, 0, 0.5);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
}

.modal-content {
  background-color: white;
  border-radius: 8px;
  width: 90%;
  max-width: 500px;
  max-height: 90vh;
  overflow-y: auto;
  box-shadow: 0 4px 24px rgba(0, 0, 0, 0.2);
}

.modal-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 24px;
  border-bottom: 1px solid #eee;
}

.modal-header h3 {
  margin: 0;
  color: #2c3e50;
}

.close-btn {
  background: none;
  border: none;
  font-size: 1.5rem;
  cursor: pointer;
  color: #7f8c8d;
}

.modal-body {
  padding: 24px;
}

.modal-footer {
  padding: 16px 24px;
  border-top: 1px solid #eee;
  display: flex;
  justify-content: flex-end;
  gap: 12px;
}

.form-group {
  margin-bottom: 16px;
}

.form-group label {
  display: block;
  margin-bottom: 8px;
  font-weight: 500;
  color: #34495e;
}

.form-group input,
.form-group textarea {
  width: 100%;
  padding: 10px 12px;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 0.9rem;
}

.warning-box {
  margin-top: 16px;
  padding: 16px;
  background-color: #fff8e1;
  border-left: 4px solid #f39c12;
  margin-bottom: 16px;
}

.warning-box p {
  margin-top: 0;
  color: #d35400;
  font-weight: 500;
}

.warning-box ul {
  margin-bottom: 0;
  padding-left: 24px;
}

.warning-box li {
  margin-bottom: 4px;
  color: #e67e22;
}

.cancel-btn {
  padding: 8px 16px;
  background-color: #ecf0f1;
  color: #7f8c8d;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.cancel-btn:hover {
  background-color: #bdc3c7;
}

.confirm-btn {
  padding: 8px 16px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  color: white;
  font-weight: 500;
  transition: background-color 0.2s;
}

.confirm-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.confirm-btn.ban {
  background-color: #f39c12;
}

.confirm-btn.ban:hover:not(:disabled) {
  background-color: #d35400;
}

.confirm-btn.unban {
  background-color: #2ecc71;
}

.confirm-btn.unban:hover:not(:disabled) {
  background-color: #27ae60;
}

.confirm-btn.delete {
  background-color: #e74c3c;
}

.confirm-btn.delete:hover:not(:disabled) {
  background-color: #c0392b;
}

/* 响应式调整 */
@media (max-width: 768px) {
  .action-buttons {
    flex-direction: column;
  }
  
  .filter-bar {
    flex-direction: column;
    align-items: stretch;
  }
  
  .filter-options select {
    width: 100%;
  }
}
</style> 