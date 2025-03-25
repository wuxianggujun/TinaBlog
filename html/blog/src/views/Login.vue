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
      if (!userData.uuid) {
        console.error('错误: 保存用户数据缺少uuid');
        return false;
      }
      
      // 保存必要的用户信息
      localStorage.setItem('user_uuid', userData.uuid);
      localStorage.setItem('username', userData.username);
      localStorage.setItem('display_name', userData.display_name || userData.username);
      localStorage.setItem('isLoggedIn', 'true');
      
      // 如果响应中含有token，也保存起来（可选项）
      if (userData.token) {
        localStorage.setItem('token', userData.token);
      }
      
      // 通知其他组件登录状态变化
      eventBus.emit('login-state-changed', true);

      // 检查是否有重定向参数
      const redirect = this.$route.query.redirect;
      // 如果有重定向参数，则跳转到指定页面，否则跳转到首页
      this.$router.push(redirect || '/');
      return true;
    },

    async handleLogin() {
      try {
        // 添加auth_type参数，用于选择返回token的方式
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

        if (data.success) {
          // 登录成功,保存用户数据
          const userData = data.data || {};
          
          // 检查必要字段
          if (!userData.uuid || !userData.username) {
            console.error('登录成功但缺少必要用户信息:', data);
            alert('登录失败，服务器返回的用户信息不完整');
            return;
          }
          
          // 保存用户数据
          const saved = this.saveUserAuth(userData);
          if (saved) {
            console.log('用户数据保存成功');
          } else {
            console.error('用户数据保存失败');
            alert('登录成功但保存用户信息失败');
          }
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
        // 添加auth_type参数
        const response = await fetch('/api/auth/register', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            username: this.registerForm.username,
            email: this.registerForm.email,
            password: this.registerForm.password,
            display_name: this.registerForm.username, // 默认显示名与用户名相同
            auth_type: 'both' // 同时在Cookie和响应体中返回token
          }),
          credentials: 'include' // 确保接收和发送Cookie
        });

        const data = await response.json();
        console.log('注册响应:', data);

        if (data.success) {
          const userData = data.data || {};
          
          // 检查必要字段
          if (!userData.uuid || !userData.username) {
            console.error('注册成功但缺少必要用户信息:', data);
            alert('注册成功但无法自动登录，请手动登录');
            this.activeTab = 'login';
            this.resetForms();
            return;
          }
          
          // 保存用户信息并自动登录
          const saved = this.saveUserAuth(userData);
          if (saved) {
            alert('注册成功并已自动登录！');
          } else {
            alert('注册成功但自动登录失败，请手动登录');
            this.activeTab = 'login';
            this.resetForms();
          }
        } else {
          alert(data.message || '注册失败，请稍后重试');
        }
      } catch (error) {
        console.error('注册出错：', error);
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
