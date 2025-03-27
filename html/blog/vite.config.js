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
    // 确保单页应用路由工作正常
    historyApiFallback: {
      rewrites: [
        { from: /^\/.*$/, to: '/index.html' }
      ]
    }
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
