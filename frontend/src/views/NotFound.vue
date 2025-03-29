<template>
  <div class="not-found-container">
    <div class="not-found-content">
      <h1>404</h1>
      <h2>页面未找到</h2>
      <p>抱歉，您访问的页面不存在或已被移除。</p>
      <router-link to="/" class="home-button">返回首页</router-link>
    </div>
  </div>
</template>

<script>
export default {
  name: 'NotFound',
  
  created() {
    // 更新页面标题
    document.title = '404 页面未找到 - Tina博客';
  }
}
</script>

<style scoped>
.not-found-container {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 70vh;
  padding: 2rem;
  text-align: center;
}

.not-found-content {
  max-width: 500px;
  padding: 2rem;
  background-color: white;
  border-radius: 8px;
  box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
}

h1 {
  font-size: 6rem;
  margin: 0;
  color: #6366f1;
  line-height: 1;
}

h2 {
  font-size: 1.8rem;
  margin-top: 0.5rem;
  margin-bottom: 1.5rem;
  color: #333;
}

p {
  font-size: 1.1rem;
  color: #666;
  margin-bottom: 2rem;
}

.home-button {
  display: inline-block;
  padding: 0.75rem 1.5rem;
  background-color: #6366f1;
  color: white;
  text-decoration: none;
  border-radius: 4px;
  font-weight: 500;
  transition: background-color 0.2s;
}

.home-button:hover {
  background-color: #4f46e5;
}
</style> 