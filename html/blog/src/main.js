import { createApp } from 'vue'
import App from './App.vue'
import router from './router'

// 确保eventBus在全局可用
console.log('初始化事件总线...');
window.eventBus = {
  _events: {},
  emit(event, ...args) {
    console.log(`[EventBus] 触发事件: ${event}`);
    if (this._events[event]) {
      this._events[event].forEach(callback => {
        try {
          callback(...args);
        } catch (error) {
          console.error(`[EventBus] 事件处理器错误: ${error.message}`);
        }
      });
    }
  },
  on(event, callback) {
    console.log(`[EventBus] 添加监听器: ${event}`);
    if (!this._events[event]) {
      this._events[event] = [];
    }
    if (!this._events[event].includes(callback)) {
      this._events[event].push(callback);
    }
  },
  off(event, callback) {
    console.log(`[EventBus] 移除监听器: ${event}`);
    if (this._events[event]) {
      this._events[event] = this._events[event].filter(cb => cb !== callback);
    }
  }
};

// 可以安全地重新赋值eventBus，以防其他文件已导入
import eventBus from './utils/eventBus';
if (eventBus && typeof eventBus !== 'undefined') {
  console.log('覆盖utils/eventBus');
  Object.assign(eventBus, window.eventBus);
}

// 允许直接使用eventBus而不通过window
globalThis.eventBus = window.eventBus;

// 创建Vue应用
console.log('创建Vue应用...');
const app = createApp(App)

// 添加全局错误处理
app.config.errorHandler = (err, vm, info) => {
  console.error('Vue全局错误:', err);
  console.error('错误信息:', info);
  
  // 记录更多上下文信息
  if (vm) {
    try {
      console.error('组件名称:', vm.$options ? vm.$options.name || '未命名组件' : '未知组件');
      console.error('组件数据:', JSON.stringify(vm.$data || {}).substring(0, 200));
    } catch (e) {
      console.error('无法获取组件信息:', e.message);
    }
  }
};

// 注册全局自定义指令（用于调试）
app.directive('log-render', {
  mounted(el, binding) {
    console.log(`组件挂载: ${binding.value || '未命名'}`);
  }
});

// 启用性能跟踪和开发工具（仅开发环境）
app.config.performance = process.env.NODE_ENV === 'development'
app.config.devtools = process.env.NODE_ENV === 'development'

// 使用路由插件
app.use(router)

// 挂载应用
console.log('挂载Vue应用...');
const vueApp = app.mount('#app')

// 简单检查辅助方法
window.checkVueStatus = () => {
  const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
  const hasToken = !!localStorage.getItem('token');
  const username = localStorage.getItem('username') || '未登录';
  
  console.log('应用状态检查:', { isLoggedIn, hasToken, username });
  
  return { isLoggedIn, hasToken, username }
}
