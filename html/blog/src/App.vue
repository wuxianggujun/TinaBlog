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
  max-width: 1920px;
  margin: 0 auto;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem 2rem;
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
