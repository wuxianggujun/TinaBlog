import { createApp } from 'vue'
import App from './App.vue'
import router from './router'

// 创建Vue应用
const app = createApp(App)

// 添加全局错误处理
app.config.errorHandler = (err, vm, info) => {
  console.error('Vue全局错误:', err);
  console.error('错误信息:', info);
  
  // 记录更多上下文信息，但避免尝试访问可能为null的对象
  if (vm) {
    console.error('错误组件名称:', vm.$options ? vm.$options.name : '未知组件');
  }
};

// 启用性能跟踪和开发工具（仅开发环境）
app.config.performance = process.env.NODE_ENV === 'development'
app.config.devtools = process.env.NODE_ENV === 'development'

// 使用路由插件
app.use(router)

// 挂载应用
const vueApp = app.mount('#app')

// 简单检查辅助方法
window.checkVueStatus = () => {
  return {
    isLoggedIn: localStorage.getItem('isLoggedIn') === 'true',
    username: localStorage.getItem('username') || '未登录'
  }
}
