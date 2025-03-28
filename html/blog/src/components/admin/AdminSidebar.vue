<template>
  <div class="sidebar">
    <div class="sidebar-header">
      <h3>文章管理系统</h3>
    </div>
    <nav class="sidebar-nav">
      <div 
        :class="['nav-item', { active: modelValue === 'dashboard' }]"
        @click="$emit('update:modelValue', 'dashboard')"
      >
        <span class="nav-icon">📊</span>
        <span class="nav-text">数据概览</span>
      </div>
      <div 
        :class="['nav-item', { active: modelValue === 'articles' }]"
        @click="$emit('update:modelValue', 'articles')"
      >
        <span class="nav-icon">📝</span>
        <span class="nav-text">文章管理</span>
      </div>
      <div 
        :class="['nav-item', { active: modelValue === 'comments' }]"
        @click="$emit('update:modelValue', 'comments')"
      >
        <span class="nav-icon">💬</span>
        <span class="nav-text">评论管理</span>
      </div>
      <div 
        :class="['nav-item', { active: modelValue === 'categories' }]"
        @click="$emit('update:modelValue', 'categories')"
      >
        <span class="nav-icon">🏷️</span>
        <span class="nav-text">分类管理</span>
      </div>
    </nav>
    <div class="sidebar-footer">
      <router-link to="/" class="back-link">
        <span class="back-icon">🏠</span>
        <span class="back-text">返回主页</span>
      </router-link>
    </div>
  </div>
</template>

<script>
export default {
  name: 'AdminSidebar',
  props: {
    modelValue: {
      type: String,
      required: true
    }
  }
}
</script>

<style scoped>
.sidebar {
  width: 220px;
  background-color: #2c3e50;
  color: #fff;
  height: 100vh;
  display: flex;
  flex-direction: column;
  position: fixed;
  left: 0;
  top: 0;
  z-index: 100;
}

.sidebar-header {
  padding: 20px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.sidebar-header h3 {
  margin: 0;
  font-size: 1.2rem;
  font-weight: 500;
}

.sidebar-nav {
  flex: 1;
  padding: 20px 0;
}

.nav-item {
  padding: 12px 20px;
  display: flex;
  align-items: center;
  cursor: pointer;
  transition: all 0.3s;
}

.nav-item:hover {
  background-color: rgba(255, 255, 255, 0.1);
}

.nav-item.active {
  background-color: rgba(255, 255, 255, 0.2);
  border-left: 4px solid #3498db;
}

.nav-icon {
  margin-right: 12px;
  font-size: 1.2rem;
}

.sidebar-footer {
  padding: 15px 20px;
  border-top: 1px solid rgba(255, 255, 255, 0.1);
}

.back-link {
  display: flex;
  align-items: center;
  color: #fff;
  text-decoration: none;
  opacity: 0.7;
  transition: opacity 0.3s;
}

.back-link:hover {
  opacity: 1;
}

.back-icon {
  margin-right: 8px;
}

@media (max-width: 768px) {
  .sidebar {
    width: 60px;
  }
  
  .sidebar-header h3,
  .nav-text,
  .back-text {
    display: none;
  }
  
  .nav-icon,
  .back-icon {
    margin-right: 0;
    font-size: 1.5rem;
  }
  
  .nav-item,
  .sidebar-footer {
    justify-content: center;
  }
}
</style> 