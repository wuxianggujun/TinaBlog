<script>
import { RouterLink, RouterView } from 'vue-router'
import './assets/base.css'
import eventBus from './utils/eventBus';

export default {
  data() {
    return {
      isLoggedIn: false,
      username: '',
      showDropdown: false,
      loginValidationInterval: null
    }
  },
  mounted() {
    // 检查登录状态
    this.checkLoginStatus();
    
    // 如果已登录但在登录页面，自动重定向
    const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
    if (isLoggedIn && this.$route.path === '/login') {
      // 获取重定向参数，如果有则跳转
      const redirect = this.$route.query.redirect || '/';
      this.$router.replace(redirect);
    }
    
    // 监听登录状态变化事件
    window.addEventListener('storage', this.handleStorageChange);
    
    // 监听用户信息更新事件
    eventBus.on('user-info-updated', this.handleUserInfoUpdate);
    
    // 立即验证token有效性
    const token = localStorage.getItem('token');
    if (isLoggedIn && token) {
      this.checkTokenValidity(token);
    }
    
    // 定期验证登录状态的一致性
    this.validateLoginState();

    // 调试：检查路由视图引用 - 添加延时确保组件已加载
    setTimeout(() => {
      console.log('路由视图引用:', this.$refs.routerView);
      if (this.$refs.routerView) {
        console.log('路由视图组件名称:', this.$refs.routerView.$options.name);
        console.log('showPublishDialog方法存在:', typeof this.$refs.routerView.showPublishDialog === 'function');
      }
    }, 1000);
  },
  beforeUnmount() {
    window.removeEventListener('storage', this.handleStorageChange);
    if (this.loginValidationInterval) {
      clearInterval(this.loginValidationInterval);
    }
    // 移除事件监听
    eventBus.off('user-info-updated', this.handleUserInfoUpdate);
  },
  methods: {
    checkLoginStatus() {
      console.log('检查登录状态...');
      const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
      const username = localStorage.getItem('username');
      console.log('从localStorage获取的状态:', {
        isLoggedIn,
        username
      });
      
      this.isLoggedIn = isLoggedIn;
      this.username = username;
      
      console.log('更新后的状态:', {
        isLoggedIn: this.isLoggedIn,
        username: this.username
      });
    },
    handleStorageChange(event) {
      if (event.key === 'isLoggedIn' || event.key === 'username') {
        this.checkLoginStatus();
      }
    },
    toggleDropdown() {
      this.showDropdown = !this.showDropdown;
    },
    hideDropdown() {
      this.showDropdown = false;
    },
    async handleLogout() {
      try {
        // 调用登出API
        const token = localStorage.getItem('token');
        if (token) {
          const response = await fetch('/api/auth/logout', {
            method: 'POST',
            headers: {
              'Authorization': `Bearer ${token}`,
              'Content-Type': 'application/json'
            }
          });
          
          const data = await response.json();
          if (!response.ok) {
            console.warn('登出API调用失败:', data.message);
          }
        }
      } catch (error) {
        console.error('登出过程中发生错误:', error);
      } finally {
        // 无论API调用成功与否，都清除本地存储
        localStorage.removeItem('token');
        localStorage.removeItem('username');
        localStorage.removeItem('isLoggedIn');
        
        // 更新状态
        this.isLoggedIn = false;
        this.username = '';
        this.showDropdown = false;
        
        // 如果在个人页面，跳转到首页
        if (this.$route.path.includes('/profile')) {
          this.$router.push('/');
        }
      }
    },
    validateLoginState() {
      // 每5分钟检查一次登录状态的一致性
      this.loginValidationInterval = setInterval(() => {
        const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
        const token = localStorage.getItem('token');
        
        // 检查登录状态一致性
        if (isLoggedIn && (!token || token.trim() === '')) {
          console.warn('登录状态不一致或token无效，已清除登录状态');
          localStorage.removeItem('isLoggedIn');
          localStorage.removeItem('username');
          localStorage.removeItem('token');
          this.checkLoginStatus(); // 更新组件状态
        }
        
        // 如果登录了，立即检查token有效性
        if (isLoggedIn && token) {
          this.checkTokenValidity(token);
        }
      }, 5 * 60 * 1000); // 5分钟
    },
    
    async checkTokenValidity(token) {
      try {
        // 调用验证token API
        const response = await fetch('/api/auth/verify', {
          method: 'GET',
          headers: {
            'Authorization': `Bearer ${token}`
          }
        });
        
        const data = await response.json();
        
        // 如果返回401，说明token无效
        if (response.status === 401) {
          console.warn('Token已失效，清除登录状态');
          localStorage.removeItem('token');
          localStorage.removeItem('username');
          localStorage.removeItem('isLoggedIn');
          this.checkLoginStatus();
        } else if (data.status === 'success') {
          // 正常响应，token有效
          console.log('Token有效，用户已登录');
          // 更新用户信息
          if (data.data && data.data.user) {
            this.username = data.data.user.username;
            this.isLoggedIn = true;
          }
        } else {
          // 其他错误情况
          console.warn('Token验证失败:', data.message);
          localStorage.removeItem('token');
          localStorage.removeItem('username');
          localStorage.removeItem('isLoggedIn');
          this.checkLoginStatus();
        }
      } catch (error) {
        console.error('验证token时出错:', error);
        // 连接错误不清除token，因为可能是网络问题
      }
    },
    handlePublish() {
      try {
        console.log('触发发布文章事件');
        // 使用事件总线触发发布对话框
        eventBus.emit('openPublishDialog');
      } catch (error) {
        console.error('触发发布事件失败:', error);
      }
    },
    handleUserInfoUpdate(userData) {
      console.log('收到用户信息更新事件:', userData);
      this.isLoggedIn = true;
      this.username = userData.username || userData.display_name;
      console.log('更新后的登录状态:', {
        isLoggedIn: this.isLoggedIn,
        username: this.username
      });
    }
  }
}
</script>

<template>
  <div class="app" @click="hideDropdown">
    <nav class="nav">
      <div class="nav-container">
        <!-- 左侧区域：品牌和导航链接 -->
        <div class="nav-left">
          <router-link to="/" class="nav-brand">Tina Blog</router-link>
          <div class="nav-links">
            <router-link to="/">首页</router-link>
            <router-link to="/categories">分类</router-link>
            <router-link to="/archives">归档</router-link>
          </div>
        </div>
        
        <!-- 右侧功能区 -->
        <div class="nav-right">
          <!-- 仅在非创建文章页面显示"创作"按钮 -->
          <router-link v-if="isLoggedIn && $route.path !== '/create'" to="/create" class="create-btn">
            <svg viewBox="0 0 24 24" width="16" height="16" style="margin-right: 5px;">
              <path d="M14.06 9.02l.92.92L5.92 19H5v-.92l9.06-9.06M17.66 3c-.25 0-.51.1-.7.29l-1.83 1.83 3.75 3.75 1.83-1.83c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.2-.2-.45-.29-.71-.29zm-3.6 3.19L3 17.25V21h3.75L17.81 9.94l-3.75-3.75z" fill="currentColor"/>
            </svg>
            创作
          </router-link>
          
          <!-- 在创建文章页面显示"发布文章"按钮 -->
          <button v-if="isLoggedIn && $route.path === '/create'" @click="handlePublish" class="publish-btn">
            <svg viewBox="0 0 1024 1024" width="16" height="16" style="margin-right: 5px;">
              <path d="M725.333333 362.666667h42.666667a128 128 0 0 1 128 128v256a128 128 0 0 1-128 128H256a128 128 0 0 1-128-128V490.666667a128 128 0 0 1 128-128h42.666667v-21.333334a213.333333 213.333333 0 0 1 426.666666 0v21.333334z m-42.666666 0v-21.333334a170.666667 170.666667 0 0 0-341.333334 0v21.333334h341.333334z m-384 170.666666a42.666667 42.666667 0 1 0 85.333333 0 42.666667 42.666667 0 0 0-85.333333 0z m170.666666 0a42.666667 42.666667 0 1 0 85.333334 0 42.666667 42.666667 0 0 0-85.333334 0z m170.666667 0a42.666667 42.666667 0 1 0 85.333333 0 42.666667 42.666667 0 0 0-85.333333 0z" fill="currentColor"></path>
            </svg>
            发布文章
          </button>
          
          <!-- 登录/用户信息 -->
          <router-link v-if="!isLoggedIn" to="/login" class="login-btn">登录</router-link>
          <div v-else class="user-menu" @click.stop="toggleDropdown">
            <div class="user-avatar">
              <span class="username">{{ username }}</span>
              <i class="dropdown-icon"></i>
            </div>
            <div v-show="showDropdown" class="dropdown-menu">
              <router-link to="/profile" class="dropdown-item">个人信息</router-link>
              <router-link to="/profile/password" class="dropdown-item">修改密码</router-link>
              <div class="dropdown-divider"></div>
              <div class="dropdown-item logout" @click="handleLogout">退出登录</div>
            </div>
          </div>
        </div>
      </div>
    </nav>

    <main class="main">
      <router-view ref="routerView" @login-state-changed="checkLoginStatus" />
    </main>

    <footer class="footer">
      <div class="footer-content">
        <p>© 2024 Tina Blog. All rights reserved.</p>
        <p><a href="https://beian.miit.gov.cn/" target="_blank">浙ICP备xxxxxxxx号-1</a></p>
      </div>
    </footer>
  </div>
</template>

<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

html, body {
  width: 100%;
  height: 100%;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen,
    Ubuntu, Cantarell, 'Open Sans', 'Helvetica Neue', sans-serif;
  background-color: #f5f5f5;
  color: #333;
  line-height: 1.6;
}

.app {
  width: 100%;
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}

.nav {
  width: 100%;
  background: white;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  position: sticky;
  top: 0;
  z-index: 100;
}

.nav-container {
  width: 100%;
  padding: 0 0.5rem;
  height: 60px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.nav-left {
  display: flex;
  align-items: center;
}

.nav-brand {
  font-size: 1.5rem;
  font-weight: bold;
  color: #6366f1;
  text-decoration: none;
  margin-right: 2rem;
}

.nav-links {
  display: flex;
  gap: 2rem;
  align-items: center;
}

.nav-right {
  display: flex;
  gap: 1rem;
  align-items: center;
}

.nav-links a {
  color: #4a4a4a;
  text-decoration: none;
  font-weight: 500;
  transition: color 0.2s;
  line-height: 1;
  padding: 0.5rem 0;
}

.nav-links a:hover,
.nav-links a.router-link-active {
  color: #6366f1;
}

.main {
  flex: 1;
  width: 100%;
  display: flex;
  flex-direction: column;
  min-height: 0;
  overflow-y: auto;
}

.footer {
  width: 100%;
  background: white;
  padding: 1rem 0;
  box-shadow: 0 -2px 4px rgba(0, 0, 0, 0.1);
}

.footer-content {
  width: 100%;
  padding: 0 2rem;
  text-align: center;
  line-height: 1.4;
}

.footer-content p {
  margin: 0.25rem 0;
  color: #666;
  font-size: 0.85rem;
}

.footer-content a {
  color: #666;
  text-decoration: none;
  transition: color 0.2s;
}

.footer-content a:hover {
  color: #6366f1;
}

.create-btn, .login-btn, .publish-btn {
  padding: 0.5rem 1rem !important;
  border-radius: 4px;
  transition: background-color 0.2s;
  text-decoration: none;
  display: flex;
  align-items: center;
}

.publish-btn {
  background-color: #6366f1;
  color: white !important;
}

.publish-btn:hover {
  background-color: #4f46e5;
}

.login-btn {
  background-color: #6366f1;
  color: white !important;
}

.login-btn:hover {
  background-color: #4f46e5;
}

.nav-right .login-btn.router-link-active {
  background-color: #4f46e5;
  color: white !important;
}

.user-menu {
  position: relative;
}

.user-avatar {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  cursor: pointer;
  padding: 0.5rem 1rem;
  border-radius: 4px;
  background-color: #6366f1;
  color: white;
  transition: background-color 0.2s;
}

.user-avatar:hover {
  background-color: #4f46e5;
}

.username {
  font-weight: 500;
}

.dropdown-icon {
  display: inline-block;
  width: 0;
  height: 0;
  border-left: 5px solid transparent;
  border-right: 5px solid transparent;
  border-top: 5px solid white;
  margin-left: 5px;
}

.dropdown-menu {
  position: absolute;
  top: calc(100% + 5px);
  right: 0;
  background: white;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
  border-radius: 4px;
  min-width: 160px;
  z-index: 100;
  overflow: hidden;
  transform-origin: top right;
  animation: dropdown-appear 0.2s ease-out;
}

@keyframes dropdown-appear {
  from {
    opacity: 0;
    transform: scale(0.9);
  }
  to {
    opacity: 1;
    transform: scale(1);
  }
}

.dropdown-item {
  display: block;
  width: 100%;
  padding: 0.75rem 1rem;
  color: #333;
  text-decoration: none;
  transition: background-color 0.2s;
  font-size: 0.95rem;
  text-align: left;
}

.dropdown-item:hover {
  background-color: #f0f0f0;
}

.dropdown-divider {
  height: 1px;
  background-color: #e0e0e0;
  margin: 0.25rem 0;
}

.dropdown-item.logout {
  color: #dc2626;
  cursor: pointer;
  text-align: left;
}

.dropdown-item.logout:hover {
  background-color: #fee2e2;
}

@media (max-width: 768px) {
  .nav-container {
    flex-direction: column;
    gap: 1rem;
    padding: 1rem;
    height: auto;
  }

  .nav-left {
    flex-direction: column;
    width: 100%;
  }

  .nav-brand {
    margin-right: 0;
    margin-bottom: 0.5rem;
  }

  .nav-links, .nav-right {
    justify-content: center;
    flex-wrap: wrap;
    gap: 1rem;
    width: 100%;
  }
}

.create-btn {
  background-color: #10b981;
  color: white !important;
}

.create-btn:hover {
  background-color: #059669;
}
</style>
