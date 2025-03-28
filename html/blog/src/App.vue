<script>
import { RouterLink, RouterView } from 'vue-router'
import './assets/base.css'
import eventBus from './utils/eventBus';
import { watchEffect } from 'vue';

export default {
  data() {
    return {
      isLoggedIn: localStorage.getItem('isLoggedIn') === 'true',
      username: localStorage.getItem('username') || '',
      showDropdown: false,
      loginValidationInterval: null,
      forceUpdateFlag: 0,  // 新增强制更新标志
      isArticleDetailPage: false  // 新增文章详情页面标志
    }
  },
  computed: {
    // 添加计算属性以便于应用响应式更新
    userDisplayName() {
      return this.username || localStorage.getItem('username') || '';
    },
    isUserLoggedIn() {
      return this.isLoggedIn || localStorage.getItem('isLoggedIn') === 'true';
    }
  },
  watch: {
    // 监听登录状态变化，确保UI立即更新
    isLoggedIn(newValue) {
      console.debug('[App] 登录状态变化:', newValue);
      // 强制更新UI
      this.$forceUpdate();
    },
    // 监听路由变化，更新文章详情页状态
    '$route'(to) {
      console.debug('[App] 路由变化:', to.path);
      this.checkIsArticleDetailPage();
    }
  },
  created() {
    console.debug('[App] created生命周期钩子');
    
    // 不要在created钩子中调用可能访问未初始化对象的方法
    // 只设置最基本的数据
    this.isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
    this.username = localStorage.getItem('username') || '';
    
    // 检查是否是文章详情页
    this.checkIsArticleDetailPage();
  },
  mounted() {
    console.debug('[App] mounted生命周期钩子');
    
    // 在mounted中进行所有复杂操作，此时组件已完全初始化
    
    // 注册localStorage变化监听器
    window.addEventListener('storage', this.handleStorageChange);
    
    // 延迟处理，确保Vue实例完全初始化
    this.$nextTick(() => {
      // 现在可以安全地检查登录状态和验证token
      try {
        this.checkLoginStatus();
        this.verifyToken();
      } catch (error) {
        console.error('[App] 初始化检查登录状态出错:', error);
      }
      
      // 设置事件监听
      if (window.eventBus) {
        window.eventBus.on('login-state-changed', this.handleLoginStateChanged);
      }
    });

    // 如果已登录但在登录页面，自动重定向
    const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
    if (isLoggedIn && this.$route.path === '/login') {
      // 获取重定向参数，如果有则跳转
      const redirect = this.$route.query.redirect || '/';
      this.$router.replace(redirect);
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
    // 清理事件监听器
    window.removeEventListener('storage', this.handleStorageChange);
    
    if (window.eventBus) {
      window.eventBus.off('login-state-changed', this.handleLoginStateChanged);
    }
    if (this.loginValidationInterval) {
      clearInterval(this.loginValidationInterval);
    }
  },
  methods: {
    // 添加一个安全的事件处理器
    handleLoginStateChanged() {
      console.debug('[App] 收到登录状态变更事件');
      // 使用setTimeout避免在事件处理过程中可能出现的问题
      setTimeout(() => {
        this.checkLoginStatus();
      }, 0);
    },
    
    handleStorageChange(event) {
      if (event.key === 'isLoggedIn' || event.key === 'token' || event.key === 'username') {
        console.debug('[App] localStorage变化:', event.key);
        // 安全地调用checkLoginStatus
        setTimeout(() => {
          this.checkLoginStatus();
        }, 0);
      }
    },
    
    checkLoginStatus() {
      // 使用try-catch包裹整个方法
      try {
        console.debug('[App] 检查登录状态...');
        
        // 只访问本地存储和简单数据，不访问复杂对象
        const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
        const username = localStorage.getItem('username');
        const token = localStorage.getItem('token');
        
        console.debug('[App] 从localStorage获取的状态:', { isLoggedIn, username, hasToken: !!token });
        
        // 更新组件数据
        this.isLoggedIn = isLoggedIn && !!token;
        this.username = this.isLoggedIn ? (username || '') : '';
        
        console.debug('[App] 更新后的状态:', { isLoggedIn: this.isLoggedIn, username: this.username });
      } catch (error) {
        console.error('[App] 检查登录状态时出错:', error);
      }
    },
    
    // 添加token验证方法，确保token有效
    verifyToken() {
      console.debug('[App] 验证token...');
      
      // 从localStorage获取token
      const token = localStorage.getItem('token');
      const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
      const username = localStorage.getItem('username');
      
      console.debug('[App] 当前缓存状态:', { isLoggedIn, hasToken: !!token, username });
      
      // 如果没有登录状态，不需要验证token
      if (!isLoggedIn) {
        console.debug('[App] 用户未登录，跳过token验证');
        return;
      }
      
      // 调用验证API检查登录状态
      fetch('/api/auth/verify', {
        method: 'GET',
        headers: {
          // 如果有token，将其添加到请求头
          ...(token ? { 'Authorization': `Bearer ${token}` } : {}),
        },
        credentials: 'include' // 确保发送cookie
      })
      .then(response => {
        console.debug('[App] 验证请求状态码:', response.status);
        
        if (response.status === 200) {
          // 成功状态，返回JSON
          return response.json().then(data => {
            console.debug('[App] 验证响应:', data);
            
            if (data.code === 0 && data.status === 'success') {
              console.debug('[App] 登录有效');
              
              // 确保localStorage状态一致
              localStorage.setItem('isLoggedIn', 'true');
              
              // 如果服务器返回了用户信息，更新本地存储
              if (data.data && data.data.user) {
                const userData = data.data.user;
                localStorage.setItem('username', userData.username);
                if (userData.uuid) {
                  localStorage.setItem('user_uuid', userData.uuid);
                }
                
                // 如果localStorage中无token但服务端认证成功，创建简单token
                if (!token) {
                  const simpleToken = `${userData.uuid || Date.now()}_${Date.now()}`;
                  console.debug('[App] 创建简单token:', simpleToken);
                  localStorage.setItem('token', simpleToken);
                }
                
                // 更新本地状态
                this.isLoggedIn = true;
                this.username = userData.username;
              }
              
              return;
            }
            
            // 不成功，清理登录状态
            console.warn('[App] 验证失败(响应成功但状态不是success)');
            this.clearLoginState();
          });
        } else if (response.status === 401) {
          // 401表示未登录或token无效
          console.warn('[App] 验证响应: 401 未授权');
          this.clearLoginState();
        } else {
          // 其他状态码，可能是服务器错误
          console.error('[App] 验证返回意外状态码:', response.status);
          // 网络/服务器问题不清理登录状态
        }
      })
      .catch(error => {
        console.error('[App] 验证token出错:', error);
        // 网络错误不清除登录状态
      });
    },
    
    // 添加清除登录状态的方法
    clearLoginState() {
      localStorage.removeItem('isLoggedIn');
      localStorage.removeItem('username');
      localStorage.removeItem('token');
      localStorage.removeItem('user_uuid');
      
      this.isLoggedIn = false;
      this.username = '';
      
      // 强制刷新UI
      this.$forceUpdate();
    },
    
    // 新增方法：直接获取当前登录状态
    getLoginStatus() {
      return {
        isLoggedIn: this.isLoggedIn || localStorage.getItem('isLoggedIn') === 'true',
        username: this.username || localStorage.getItem('username') || '',
        forceUpdateFlag: this.forceUpdateFlag
      };
    },
    
    // 添加能被子组件调用的调试方法
    debugLogin() {
      console.debug('[App] 调试登录状态');
      console.debug('[App] 当前状态:', {
        isLoggedIn: this.isLoggedIn,
        username: this.username,
        isUserLoggedIn: this.isUserLoggedIn,
        userDisplayName: this.userDisplayName,
        forceUpdateFlag: this.forceUpdateFlag
      });
      
      // 本地存储检查
      const localStorage_isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
      const localStorage_username = localStorage.getItem('username');
      console.debug('[App] 本地存储:', {
        isLoggedIn: localStorage_isLoggedIn,
        username: localStorage_username,
        token: localStorage.getItem('token') ? '存在' : '不存在',
        user_uuid: localStorage.getItem('user_uuid') || '无'
      });
      
      // DOM检查
      console.debug('[App] DOM状态:', {
        loginBtn: document.querySelector('.login-btn') ? '可见' : '不可见',
        createBtn: document.querySelector('.create-btn') ? '可见' : '不可见',
        userMenu: document.querySelector('.user-menu') ? '可见' : '不可见'
      });
      
      return {
        status: 'ok',
        loginStatus: this.isLoggedIn, 
        username: this.username
      };
    },
    
    updateLoginState(isLoggedIn, username) {
      console.debug('[App] updateLoginState被调用:', { isLoggedIn, username });
      
      this.isLoggedIn = isLoggedIn;
      this.username = username;
      
      console.debug('[App] 状态已更新，启动强制刷新');
      
      // 强制刷新视图
      this.forceUpdateFlag++;
      this.$forceUpdate();
      
      // 延迟再次刷新
      setTimeout(() => {
        console.debug('[App] 延迟刷新');
        this.forceUpdateFlag++;
        this.$forceUpdate();
      }, 100);
      
      return true;
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
        localStorage.removeItem('user_uuid');
        localStorage.removeItem('display_name');
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
          this.verifyToken();
        }
      }, 5 * 60 * 1000); // 5分钟
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
      console.debug('[App] 收到用户信息更新事件:', userData);
      console.debug('[App] 更新前的登录状态:', {
        isLoggedIn: this.isLoggedIn,
        username: this.username
      });
      
      // 强制更新用户信息和登录状态
      this.isLoggedIn = true;
      this.username = userData.username || userData.display_name || '';
      
      // 确保localStorage也同步更新
      localStorage.setItem('isLoggedIn', 'true');
      localStorage.setItem('username', this.username);
      
      console.debug('[App] 更新后的登录状态:', {
        isLoggedIn: this.isLoggedIn,
        username: this.username
      });
      
      // 使用$nextTick确保DOM更新
      this.$nextTick(() => {
        // 触发重新计算计算属性
        this.$forceUpdate();
        console.debug('[App] 视图已强制更新');
        
        // 额外调试: 检查更新后的DOM状态
        console.debug('[App] 更新后的DOM状态:', {
          isUserLoggedInComputed: this.isUserLoggedIn,
          userDisplayNameComputed: this.userDisplayName,
          navLoginButton: document.querySelector('.login-btn'),
          navUserMenu: document.querySelector('.user-menu'),
          createButton: document.querySelector('.create-btn')
        });
      });
      
      // 10ms后再次触发状态检查，确保状态已传播
      setTimeout(() => {
        console.debug('[App] 延迟检查 - 登录状态:', {
          isLoggedIn: this.isLoggedIn,
          username: this.username,
          isUserLoggedInComputed: this.isUserLoggedIn,
          userDisplayNameComputed: this.userDisplayName
        });
        this.$forceUpdate(); // 再次强制更新
      }, 10);
      
      // 更新强制刷新标志
      this.forceUpdateFlag++;
    },
    checkIsArticleDetailPage() {
      // 同时检测文章详情页和我的文章页面
      this.isArticleDetailPage = this.$route.path.includes('/article') || this.$route.path === '/my-articles';
    }
  }
}
</script>

<template>
  <div class="app" @click="hideDropdown">
    <nav class="nav">
      <div class="nav-container">
        <!-- 左侧导航 -->
        <div class="nav-left">
          <router-link to="/" class="nav-brand">Tina Blog</router-link>
          <!-- 在文章详情页面隐藏导航链接 -->
          <div class="nav-links" v-if="!isArticleDetailPage">
            <router-link to="/">首页</router-link>
            <router-link to="/categories">分类</router-link>
            <router-link to="/archives">归档</router-link>
          </div>
        </div>
        
        <!-- 右侧功能区 -->
        <div class="nav-right">
          <!-- 登录按钮 - 未登录时显示 -->
          <router-link v-if="!isLoggedIn" to="/login" class="login-btn">登录</router-link>
          
          <!-- 创作按钮 - 已登录且不在创建页面且不在文章详情页时显示 -->
          <router-link v-if="isLoggedIn && $route.path !== '/create' && !isArticleDetailPage" to="/create" class="create-btn">
            <svg viewBox="0 0 24 24" width="16" height="16" style="margin-right: 5px;">
              <path d="M14.06 9.02l.92.92L5.92 19H5v-.92l9.06-9.06M17.66 3c-.25 0-.51.1-.7.29l-1.83 1.83 3.75 3.75 1.83-1.83c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.2-.2-.45-.29-.71-.29zm-3.6 3.19L3 17.25V21h3.75L17.81 9.94l-3.75-3.75z" fill="currentColor"/>
            </svg>
            创作
          </router-link>
          
          <!-- 发布文章按钮 - 已登录且在创建页面时显示 -->
          <button v-if="isLoggedIn && $route.path === '/create'" @click="handlePublish" class="publish-btn">
            <svg viewBox="0 0 1024 1024" width="16" height="16" style="margin-right: 5px;">
              <path d="M725.333333 362.666667h42.666667a128 128 0 0 1 128 128v256a128 128 0 0 1-128 128H256a128 128 0 0 1-128-128V490.666667a128 128 0 0 1 128-128h42.666667v-21.333334a213.333333 213.333333 0 0 1 426.666666 0v21.333334z m-42.666666 0v-21.333334a170.666667 170.666667 0 0 0-341.333334 0v21.333334h341.333334z m-384 170.666666a42.666667 42.666667 0 1 0 85.333333 0 42.666667 42.666667 0 0 0-85.333333 0z m170.666666 0a42.666667 42.666667 0 1 0 85.333334 0 42.666667 42.666667 0 0 0-85.333334 0z m170.666667 0a42.666667 42.666667 0 1 0 85.333333 0 42.666667 42.666667 0 0 0-85.333333 0z" fill="currentColor"></path>
            </svg>
            发布文章
          </button>
          
          <!-- 用户菜单 - 已登录时显示 -->
          <div v-if="isLoggedIn" class="user-menu" @click.stop="toggleDropdown">
            <div class="user-avatar">
              <span class="username">{{ username }}</span>
              <i class="dropdown-icon"></i>
            </div>
            <div v-show="showDropdown" class="dropdown-menu">
              <router-link to="/profile" class="dropdown-item">个人信息</router-link>
              <router-link to="/my-articles" class="dropdown-item">我的文章</router-link>
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

<style scoped>
/* 检查是否存在可能添加黑色边框的样式 */
.header {
  /*... existing styles ... */
  outline: none; /* 防止选中时出现黑色边框 */
}

/* 修复Outline问题 */
:focus {
  outline: none !important;
}

:focus-visible {
  outline: 2px solid var(--primary-color) !important; /* 使用主题色而非黑色 */
}

/* 确保页面根元素不会被意外选中 */
#app {
  outline: none !important;
}

/* 给标题等元素添加统一的样式 */
.logo-title, .nav-brand {
  outline: none !important;
  user-select: none !important; /* 防止文本被意外选中 */
}

/* 修复Tinablog标题的边框问题 */
.nav-brand:focus, 
.nav-brand:focus-visible,
.nav-links a:focus,
.nav-links a:focus-visible,
.login-btn:focus,
.login-btn:focus-visible,
.create-btn:focus,
.create-btn:focus-visible,
.user-menu:focus,
.user-menu:focus-visible {
  outline: none !important;
  box-shadow: none !important;
  border: none !important;
}
</style>
