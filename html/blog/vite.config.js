import { fileURLToPath, URL } from 'node:url'

import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

// https://vite.dev/config/
export default defineConfig({
  plugins: [
    vue(),
    vueDevTools(),
  ],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url))
    },
  },
  server: {
    proxy: {
      '/api': {
        target: 'http://localhost:8080',
        changeOrigin: true
      }
    },
    // 配置回退，所有404的静态资源请求都指向index.html
    historyApiFallback: true
  },
  build: {
    chunkSizeWarningLimit: 1000, // 提高警告阈值到1000KB
    rollupOptions: {
      output: {
        manualChunks: {
          'markdown-editor': ['markdown-it', 'highlight.js'],
          'vue-vendor': ['vue', 'vue-router']
        }
      }
    }
  }
})
