<script setup>
import { RouterLink, RouterView } from 'vue-router'
import './assets/base.css'
</script>

<template>
  <div class="app">
    <nav class="nav">
      <div class="nav-container">
        <router-link to="/" class="nav-brand">Tina Blog</router-link>
        <div class="nav-links">
          <router-link to="/">首页</router-link>
          <router-link to="/categories">分类</router-link>
          <router-link to="/archives">归档</router-link>
          <router-link to="/about">关于</router-link>
        </div>
      </div>
    </nav>

    <main class="main">
      <router-view />
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
  padding: 1rem 2rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.nav-brand {
  font-size: 1.5rem;
  font-weight: bold;
  color: #6366f1;
  text-decoration: none;
}

.nav-links {
  display: flex;
  gap: 2rem;
}

.nav-links a {
  color: #4a4a4a;
  text-decoration: none;
  font-weight: 500;
  transition: color 0.2s;
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

@media (max-width: 768px) {
  .nav-container {
    flex-direction: column;
    gap: 1rem;
    padding: 1rem;
  }

  .nav-links {
    justify-content: center;
    flex-wrap: wrap;
  }
}
</style>
