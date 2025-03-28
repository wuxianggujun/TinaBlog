<template>
  <div class="admin-layout">
    <!-- 左侧导航栏 -->
    <div class="admin-sidebar">
      <div class="sidebar-header">
        <div class="logo">
          <span class="logo-icon">📚</span>
          <span>Tina Blog</span>
        </div>
      </div>
      
      <div class="sidebar-menu">
        <div 
          class="menu-item" 
          :class="{ active: activeMenu === 'dashboard' }"
          @click="activeMenu = 'dashboard'"
        >
          <span class="menu-icon">📊</span>
          <span class="menu-text">数据概览</span>
        </div>
        
        <div 
          class="menu-item" 
          :class="{ active: activeMenu === 'articles' }"
          @click="activeMenu = 'articles'"
        >
          <span class="menu-icon">📄</span>
          <span class="menu-text">文章管理</span>
        </div>
        
        <div 
          class="menu-item" 
          :class="{ active: activeMenu === 'comments' }"
          @click="activeMenu = 'comments'"
        >
          <span class="menu-icon">💬</span>
          <span class="menu-text">评论管理</span>
        </div>
        
        <div 
          class="menu-item" 
          :class="{ active: activeMenu === 'categories' }"
          @click="activeMenu = 'categories'"
        >
          <span class="menu-icon">🏷️</span>
          <span class="menu-text">分类管理</span>
        </div>
        
        <div 
          v-if="userInfo && userInfo.is_admin"
          class="menu-item" 
          :class="{ active: activeMenu === 'users' }"
          @click="activeMenu = 'users'"
        >
          <span class="menu-icon">👥</span>
          <span class="menu-text">用户管理</span>
        </div>
      </div>
    </div>
    
    <!-- 右侧内容区 -->
    <div class="admin-content">
      <!-- 顶部栏 -->
      <div class="admin-topbar">
        <div class="topbar-left">
          <div class="breadcrumb">
            <span class="breadcrumb-item active">管理后台</span>
            <span class="breadcrumb-separator">/</span>
            <span class="breadcrumb-item active">{{ getMenuTitle }}</span>
          </div>
        </div>
        
        <div class="topbar-right">
          <div class="user-info">
            <template v-if="userInfo">
              <img 
                v-if="userInfo.avatar" 
                :src="userInfo.avatar" 
                :alt="userInfo.display_name || userInfo.username"
                class="user-avatar"
              >
              <div v-else class="user-avatar-placeholder">
                {{ getInitials(userInfo.display_name || userInfo.username) }}
              </div>
              <div class="user-name">{{ userInfo.display_name || userInfo.username }}</div>
            </template>
          </div>
          
          <button class="exit-btn" @click="$router.push('/')">
            <span class="btn-icon">🏠</span>
            <span class="btn-text">返回首页</span>
          </button>
        </div>
      </div>
      
      <!-- 主内容区 -->
      <div class="admin-main">
        <transition name="fade" mode="out-in">
          <!-- 数据概览 -->
          <dashboard-view 
            v-if="activeMenu === 'dashboard'" 
            :articles="articles"
            :comments="comments"
            :categories="categories"
          />
          
          <!-- 文章管理 -->
          <articles-view 
            v-else-if="activeMenu === 'articles'" 
            :articles="articles"
            @reload="fetchArticles"
          />
          
          <!-- 评论管理 -->
          <comments-view 
            v-else-if="activeMenu === 'comments'" 
            :comments="comments"
            :articles="articles"
            @reload="fetchComments"
          />
          
          <!-- 分类管理 -->
          <categories-view 
            v-else-if="activeMenu === 'categories'" 
            :articles="articles"
          />
          
          <!-- 用户管理 -->
          <users-view 
            v-else-if="activeMenu === 'users' && userInfo && userInfo.is_admin" 
            @reload="fetchUsers"
          />
        </transition>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';
import DashboardView from '../components/admin/DashboardView.vue';
import ArticlesView from '../components/admin/ArticlesView.vue';
import CommentsView from '../components/admin/CommentsView.vue';
import CategoriesView from '../components/admin/CategoriesView.vue';
import UsersView from '../components/admin/UsersView.vue';

export default {
  name: 'MyArticlesView',
  components: {
    DashboardView,
    ArticlesView,
    CommentsView,
    CategoriesView,
    UsersView
  },
  data() {
    return {
      activeMenu: 'dashboard',
      userInfo: null,
      articles: [],
      comments: [],
      categories: [],
      users: [],
      isLoading: false
    };
  },
  computed: {
    getMenuTitle() {
      switch (this.activeMenu) {
        case 'dashboard': return '数据概览';
        case 'articles': return '文章管理';
        case 'comments': return '评论管理';
        case 'categories': return '分类管理';
        case 'users': return '用户管理';
        default: return '';
      }
    }
  },
  created() {
    this.fetchUserInfo();
    this.fetchArticles();
    this.fetchComments();
    this.fetchCategories();
  },
  methods: {
    getInitials(name) {
      if (!name) return '';
      return name.charAt(0).toUpperCase();
    },
    
    async fetchUserInfo() {
      try {
        const response = await axios.get('/api/user/info');
        if (response.data.code === 0) {
          this.userInfo = response.data.data;
          if (this.userInfo.is_admin) {
            this.fetchUsers();
          }
        } else {
          console.error('获取用户信息失败:', response.data.message);
        }
      } catch (error) {
        console.error('获取用户信息请求错误:', error);
      }
    },
    
    async fetchArticles() {
      this.isLoading = true;
      try {
        const response = await axios.get('/api/articles/my');
        if (response.data.code === 0) {
          this.articles = response.data.data || [];
        } else {
          console.error('获取文章列表失败:', response.data.message);
          this.articles = [];
        }
      } catch (error) {
        console.error('获取文章列表请求错误:', error);
        this.articles = [];
      } finally {
        this.isLoading = false;
      }
    },
    
    async fetchComments() {
      try {
        const response = await axios.get('/api/comments');
        if (response.data.code === 0) {
          this.comments = response.data.data || [];
        } else {
          console.error('获取评论列表失败:', response.data.message);
          this.comments = [];
        }
      } catch (error) {
        console.error('获取评论列表请求错误:', error);
        this.comments = [];
      }
    },
    
    async fetchCategories() {
      try {
        const response = await axios.get('/api/categories');
        if (response.data.code === 0) {
          this.categories = response.data.data || [];
        } else {
          console.error('获取分类列表失败:', response.data.message);
          this.categories = [];
        }
      } catch (error) {
        console.error('获取分类列表请求错误:', error);
        this.categories = [];
      }
    },
    
    async fetchUsers() {
      if (!this.userInfo || !this.userInfo.is_admin) return;
      
      try {
        const response = await axios.get('/api/admin/users');
        if (response.data.code === 0) {
          this.users = response.data.data || [];
        } else {
          console.error('获取用户列表失败:', response.data.message);
          this.users = [];
        }
      } catch (error) {
        console.error('获取用户列表请求错误:', error);
        this.users = [];
      }
    }
  }
}
</script>

<style scoped>
.admin-layout {
  display: flex;
  min-height: 100vh;
  background-color: #f5f7fa;
}

/* 侧边栏样式 */
.admin-sidebar {
  width: 250px;
  background-color: #2c3e50;
  color: #fff;
  display: flex;
  flex-direction: column;
  position: fixed;
  top: 0;
  bottom: 0;
  left: 0;
  z-index: 100;
}

.sidebar-header {
  padding: 20px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.logo {
  display: flex;
  align-items: center;
  gap: 10px;
}

.logo span {
  font-size: 1.2rem;
  font-weight: 600;
}

.logo-icon {
  font-size: 1.2rem;
}

.sidebar-menu {
  padding: 20px 0;
  flex: 1;
}

.menu-item {
  display: flex;
  align-items: center;
  padding: 12px 20px;
  cursor: pointer;
  transition: all 0.3s;
}

.menu-item:hover {
  background-color: rgba(255, 255, 255, 0.1);
}

.menu-item.active {
  background-color: rgba(255, 255, 255, 0.2);
  position: relative;
}

.menu-item.active::before {
  content: '';
  position: absolute;
  left: 0;
  top: 0;
  bottom: 0;
  width: 4px;
  background-color: #3498db;
}

.menu-icon {
  margin-right: 12px;
  font-size: 1.2rem;
}

/* 内容区样式 */
.admin-content {
  flex: 1;
  margin-left: 250px;
  padding: 0;
  display: flex;
  flex-direction: column;
}

.admin-topbar {
  background-color: #fff;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.05);
  padding: 16px 24px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.breadcrumb {
  display: flex;
  align-items: center;
}

.breadcrumb-item {
  color: #34495e;
  text-decoration: none;
}

.breadcrumb-item.active {
  color: #7f8c8d;
  font-weight: 500;
}

.breadcrumb-separator {
  margin: 0 8px;
  color: #bdc3c7;
}

.topbar-right {
  display: flex;
  align-items: center;
  gap: 20px;
}

.user-info {
  display: flex;
  align-items: center;
  gap: 10px;
}

.user-avatar, .user-avatar-placeholder {
  width: 36px;
  height: 36px;
  border-radius: 50%;
  object-fit: cover;
}

.user-avatar-placeholder {
  background-color: #3498db;
  color: white;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: bold;
}

.user-name {
  font-weight: 500;
}

.exit-btn {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 8px 16px;
  background-color: #e74c3c;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.exit-btn:hover {
  background-color: #c0392b;
}

.btn-icon {
  font-size: 1.1rem;
}

.btn-text {
  font-weight: 500;
}

.admin-main {
  flex: 1;
  padding: 24px;
  overflow-x: hidden;
}

/* 页面过渡效果 */
.fade-enter-active, .fade-leave-active {
  transition: opacity 0.3s;
}

.fade-enter-from, .fade-leave-to {
  opacity: 0;
}

/* 响应式布局 */
@media (max-width: 992px) {
  .admin-sidebar {
    width: 70px;
  }
  
  .admin-content {
    margin-left: 70px;
  }
  
  .logo span, .menu-text {
    display: none;
  }
  
  .menu-item {
    justify-content: center;
    padding: 15px 0;
  }
  
  .menu-icon {
    margin-right: 0;
    font-size: 1.5rem;
  }
}

@media (max-width: 576px) {
  .admin-topbar {
    flex-direction: column;
    align-items: flex-start;
    gap: 12px;
  }
  
  .topbar-right {
    width: 100%;
    justify-content: space-between;
  }
}
</style> 