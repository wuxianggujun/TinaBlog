<template>
  <div class="login-container">
    <div class="form-container">
      <div class="form-tabs">
        <button
            :class="['tab-btn', { active: activeTab === 'login' }]"
            @click="activeTab = 'login'"
        >
          登录
        </button>
        <button
            :class="['tab-btn', { active: activeTab === 'register' }]"
            @click="activeTab = 'register'"
        >
          注册
        </button>
      </div>

      <!-- 登录表单 -->
      <form v-if="activeTab === 'login'" @submit.prevent="handleLogin" class="form">
        <div class="form-group">
          <label for="login-username">用户名</label>
          <input
              type="text"
              id="login-username"
              v-model="loginForm.username"
              placeholder="请输入用户名"
              required
          >
        </div>
        <div class="form-group">
          <label for="login-password">密码</label>
          <input
              type="password"
              id="login-password"
              v-model="loginForm.password"
              placeholder="请输入密码"
              required
          >
        </div>
        <button type="submit" class="submit-btn">登录</button>
      </form>

      <!-- 注册表单 -->
      <form v-else @submit.prevent="handleRegister" class="form">
        <div class="form-group">
          <label for="register-username">用户名</label>
          <input
              type="text"
              id="register-username"
              v-model="registerForm.username"
              placeholder="请输入用户名"
              required
          >
        </div>
        <div class="form-group">
          <label for="register-email">邮箱</label>
          <input
              type="email"
              id="register-email"
              v-model="registerForm.email"
              placeholder="请输入邮箱"
              required
          >
        </div>
        <div class="form-group">
          <label for="register-password">密码</label>
          <input
              type="password"
              id="register-password"
              v-model="registerForm.password"
              placeholder="请输入密码"
              required
          >
        </div>
        <div class="form-group">
          <label for="register-confirm-password">确认密码</label>
          <input
              type="password"
              id="register-confirm-password"
              v-model="registerForm.confirmPassword"
              placeholder="请再次输入密码"
              required
          >
        </div>
        <button type="submit" class="submit-btn">注册</button>
      </form>
    </div>
  </div>
</template>

<script>
import eventBus from '../utils/eventBus';

export default {
  name: 'Login',
  data() {
    return {
      activeTab: 'login',
      loginForm: {
        username: '',
        password: ''
      },
      registerForm: {
        username: '',
        email: '',
        password: '',
        confirmPassword: ''
      }
    }
  },
  methods: {
    saveUserAuth(userData) {
      // 保存用户数据到localStorage
      try {
        if (!userData.uuid) {
          console.error('错误: 保存用户数据缺少uuid');
          return false;
        }
        
        console.debug('[Login] 开始保存用户认证数据:', userData);
        
        // 保存所有必要的用户信息到localStorage
        localStorage.setItem('user_uuid', userData.uuid);
        localStorage.setItem('username', userData.username);
        localStorage.setItem('display_name', userData.display_name || userData.username);
        localStorage.setItem('isLoggedIn', 'true');
        
        // 如果响应中含有token，也保存起来
        if (userData.token) {
          localStorage.setItem('token', userData.token);
        }
        
        console.debug('[Login] 用户认证数据保存完成');
        
        // 发送事件通知父组件
        console.debug('[Login] 发送login-state-changed事件');
        this.$emit('login-state-changed', true);
        
        return true;
      } catch (error) {
        console.error('[Login] 保存用户认证数据出错:', error);
        return false;
      }
    },

    async handleLogin() {
      try {
        console.log('开始登录请求...');
        const response = await fetch('/api/auth/login', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            username: this.loginForm.username,
            password: this.loginForm.password,
            auth_type: 'both' // 同时在Cookie和响应体中返回token
          }),
          credentials: 'include' // 确保接收和发送Cookie
        });

        const data = await response.json();
        console.log('登录响应:', data);

        if (data.status === "success") {
          console.log('登录成功，保存用户数据...');
          
          // 清除旧数据
          localStorage.removeItem('isLoggedIn');
          localStorage.removeItem('username');
          localStorage.removeItem('token');
          localStorage.removeItem('user_uuid');
          
          // 保存新数据
          localStorage.setItem('isLoggedIn', 'true');
          localStorage.setItem('username', this.loginForm.username);
          
          // 从响应中保存token
          if (data.data && data.data.token) {
            console.log('从响应中获取到token');
            localStorage.setItem('token', data.data.token);
          } else {
            console.log('响应中没有token，尝试从cookie获取');
            try {
              const cookies = document.cookie.split(';');
              const tokenCookie = cookies.find(cookie => cookie.trim().startsWith('token='));
              if (tokenCookie) {
                const token = tokenCookie.split('=')[1];
                console.log('从cookie获取到token:', token ? '有值' : '无值');
                if (token) {
                  localStorage.setItem('token', token);
                }
              }
            } catch (e) {
              console.error('解析cookie出错:', e);
            }
          }
          
          // 保存UUID
          if (data.data && data.data.uuid) {
            localStorage.setItem('user_uuid', data.data.uuid);
          }
          
          console.log('用户数据保存完成');
          
          // 触发登录状态变更事件
          console.log('尝试触发登录状态变更事件');
          if (window.eventBus) {
            console.log('使用window.eventBus触发事件');
            try {
              window.eventBus.emit('login-state-changed');
              console.log('事件已触发');
            } catch (e) {
              console.error('触发事件出错:', e);
            }
          } else {
            console.log('window.eventBus不存在');
            // 尝试发送组件事件
            this.$emit('login-state-changed');
          }
          
          // 短暂延迟后导航，确保数据保存
          setTimeout(() => {
            const redirect = this.$route.query.redirect || '/';
            console.log('准备跳转到:', redirect);
            this.$router.push(redirect);
          }, 100);
        } else {
          alert(data.message || '登录失败，请检查用户名和密码');
        }
      } catch (error) {
        console.error('登录出错：', error);
        alert('登录失败，请稍后重试');
      }
    },
    
    async handleRegister() {
      if (this.registerForm.password !== this.registerForm.confirmPassword) {
        alert('两次输入的密码不一致！');
        return;
      }

      try {
        console.log('开始注册请求...');
        const response = await fetch('/api/auth/register', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            username: this.registerForm.username,
            email: this.registerForm.email,
            password: this.registerForm.password,
            display_name: this.registerForm.username,
            auth_type: 'both' // 同时在Cookie和响应体中返回token
          }),
          credentials: 'include' // 确保接收和发送Cookie
        });

        const data = await response.json();
        console.log('注册响应:', data);

        if (data.status === "success") {
          console.log('注册成功，保存用户数据...');
          
          // 保存基本信息
          localStorage.setItem('isLoggedIn', 'true');
          localStorage.setItem('username', this.registerForm.username);
          
          // 重要：保存token
          if (data.data && data.data.token) {
            console.log('从响应中获取到token');
            localStorage.setItem('token', data.data.token);
          } else {
            // 尝试从cookie获取
            console.log('响应中没有token，检查cookie...');
            const cookies = document.cookie.split(';');
            const tokenCookie = cookies.find(cookie => cookie.trim().startsWith('token='));
            if (tokenCookie) {
              const token = tokenCookie.split('=')[1];
              console.log('从cookie获取到token');
              localStorage.setItem('token', token);
            } else {
              console.warn('未找到token，用户可能无法访问受保护资源');
            }
          }
          
          // 保存UUID (如果有)
          if (data.data && data.data.uuid) {
            localStorage.setItem('user_uuid', data.data.uuid);
          }
          
          console.log('用户数据保存完成');
          
          // 触发登录状态变更事件
          if (window.eventBus) {
            console.log('触发登录状态变更事件');
            window.eventBus.emit('login-state-changed');
          }
          
          // 使用router导航
          const redirect = this.$route.query.redirect || '/';
          console.log('跳转到:', redirect);
          this.$router.push(redirect);
        } else {
          console.error('注册失败:', data.message);
          alert(data.message || '注册失败，请稍后重试');
        }
      } catch (error) {
        console.error('注册失败:', error);
        alert('注册失败，请稍后重试');
      }
    },
    
    resetForms() {
      this.registerForm = {
        username: '',
        email: '',
        password: '',
        confirmPassword: ''
      };
      this.loginForm = {
        username: '',
        password: ''
      };
    }
  }
}
</script>

<style scoped>
.login-container {
  width: 100%;
  min-height: 100%;
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 2rem;
  background-color: #f5f5f5;
}

.form-container {
  width: 100%;
  max-width: 400px;
  background: white;
  border-radius: 8px;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
  padding: 2rem;
}

.form-tabs {
  display: flex;
  margin-bottom: 2rem;
  border-bottom: 2px solid #eee;
}

.tab-btn {
  flex: 1;
  padding: 1rem;
  background: none;
  border: none;
  font-size: 1.1rem;
  color: #666;
  cursor: pointer;
  transition: all 0.3s;
}

.tab-btn.active {
  color: #6366f1;
  border-bottom: 2px solid #6366f1;
  margin-bottom: -2px;
}

.form {
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}

.form-group {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.form-group label {
  color: #333;
  font-size: 0.9rem;
}

.form-group input {
  padding: 0.75rem;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 1rem;
  transition: border-color 0.3s;
}

.form-group input:focus {
  outline: none;
  border-color: #6366f1;
}

.submit-btn {
  background: #6366f1;
  color: white;
  border: none;
  padding: 1rem;
  border-radius: 4px;
  font-size: 1rem;
  cursor: pointer;
  transition: background-color 0.3s;
}

.submit-btn:hover {
  background: #4f46e5;
}

@media (max-width: 768px) {
  .login-container {
    padding: 1rem;
  }

  .form-container {
    padding: 1.5rem;
  }
}
</style> 
