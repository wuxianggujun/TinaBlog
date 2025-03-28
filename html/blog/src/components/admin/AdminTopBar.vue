<template>
  <div class="top-bar">
    <div class="breadcrumb">
      <span>管理控制台</span>
      <span class="separator">/</span>
      <span>{{ getMenuTitle }}</span>
    </div>
    <div class="user-actions">
      <router-link to="/create" class="action-btn create-btn">
        <span class="btn-icon">+</span>
        <span class="btn-text">写文章</span>
      </router-link>
    </div>
  </div>
</template>

<script>
export default {
  name: 'AdminTopBar',
  props: {
    activeMenu: {
      type: String,
      required: true
    }
  },
  computed: {
    getMenuTitle() {
      const menuTitles = {
        dashboard: '数据概览',
        articles: '文章管理',
        comments: '评论管理',
        categories: '分类管理'
      };
      return menuTitles[this.activeMenu] || '';
    }
  }
}
</script>

<style scoped>
.top-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 24px;
  background-color: #fff;
  border-bottom: 1px solid #eee;
  margin-bottom: 24px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
}

.breadcrumb {
  font-size: 0.95rem;
  color: #666;
}

.separator {
  margin: 0 8px;
  color: #ccc;
}

.user-actions {
  display: flex;
  gap: 12px;
}

.action-btn {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 8px 16px;
  border-radius: 4px;
  cursor: pointer;
  transition: all 0.2s;
  text-decoration: none;
}

.create-btn {
  background-color: #3498db;
  color: white;
}

.create-btn:hover {
  background-color: #2980b9;
}

.btn-icon {
  font-size: 1.1rem;
}

@media (max-width: 768px) {
  .btn-text {
    display: none;
  }
  
  .action-btn {
    padding: 8px;
  }
}
</style> 