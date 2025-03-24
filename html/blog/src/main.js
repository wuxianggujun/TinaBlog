import { createApp } from 'vue'
import App from './App.vue'
import router from './router'

const app = createApp(App)

// 开启调试选项
app.config.performance = true
app.config.devtools = true

app.use(router)

// 将app挂载到window上，便于调试
const vueApp = app.mount('#app')
window.vueApp = vueApp
