<template>
  <div class="home-container">
    <div class="main-container">
      <!-- 左侧文章列表 -->
      <div class="content">
        <div class="content-card">
          <h2 class="section-title">最新文章</h2>
          <div class="post-list">
            <article v-for="post in posts" :key="post.id" class="post-card">
              <div class="post-header">
                <h3 class="post-title">{{ post.title }}</h3>
                <div class="post-meta">
                  <span class="post-date">{{ post.date }}</span>
                  <span class="post-category">{{ post.category }}</span>
                </div>
              </div>
              <p class="post-excerpt">{{ post.excerpt }}</p>
              <router-link :to="'/post/' + post.id" class="read-more">
                阅读更多
              </router-link>
            </article>
          </div>
        </div>
      </div>
      
      <!-- 右侧边栏 -->
      <aside class="sidebar">
        <!-- 博客标题区域 -->
        <div class="blog-header">
          <h1>Tina的个人博客</h1>
          <p>分享技术，记录生活</p>
        </div>
        
        <div class="widget about-widget">
          <h3>关于我</h3>
          <p>热爱技术，热爱生活的程序员</p>
        </div>
        
        <div class="widget category-widget">
          <h3>分类</h3>
          <ul>
            <li v-for="category in categories" :key="category.id">
              <a href="#">{{ category.name }} ({{ category.count }})</a>
            </li>
          </ul>
        </div>
      </aside>
    </div>
  </div>
</template>

<script>
export default {
  name: 'Home',
  data() {
    return {
      posts: [
        {
          id: 1,
          title: '欢迎来到我的博客',
          date: '2024-03-17',
          category: '生活随笔',
          excerpt: '这是我的第一篇博客文章，希望能够通过这个平台记录自己的成长，分享技术经验。'
        },
        // 更多文章数据
      ],
      categories: [
        { id: 1, name: '技术分享', count: 10 },
        { id: 2, name: '生活随笔', count: 5 },
        { id: 3, name: '学习笔记', count: 8 }
      ]
    }
  }
}
</script>

<style scoped>
.home-container {
  width: 100%;
  flex: 1;
  display: flex;
  justify-content: center;
  background-color: #f5f5f5;
  padding: 2rem 1rem;
  overflow-y: auto;
}

.main-container {
  width: 100%;
  display: grid;
  grid-template-columns: minmax(0, 4fr) minmax(300px, 1fr);
  gap: 2rem;
  margin-bottom: 2rem;
}

.content {
  min-width: 0;
  width: 100%;
}

.content-card {
  width: 100%;
  background: white;
  border-radius: 8px;
  padding: 1.5rem;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  display: flex;
  flex-direction: column;
}

.sidebar {
  width: 100%;
  min-width: 300px;
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}

.section-title {
  font-size: 1.5rem;
  color: #1a1a1a;
  margin: 0 0 1.5rem;
  padding-left: 0.5rem;
  border-left: 4px solid #6366f1;
}

.post-list {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  flex: 1;
}

.post-card {
  background: white;
  border-radius: 8px;
  padding: 1.25rem;
  border: 1px solid #eee;
  transition: transform 0.2s, box-shadow 0.3s;
  display: flex;
  flex-direction: column;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
}

.post-card:hover {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
}

.post-header {
  margin-bottom: 0.75rem;
}

.post-title {
  font-size: 1.3rem;
  margin: 0 0 0.5rem;
  color: #1a1a1a;
}

.post-meta {
  font-size: 0.9rem;
  color: #666;
  margin-bottom: 0.75rem;
  display: flex;
  gap: 1rem;
}

.post-excerpt {
  color: #4a4a4a;
  line-height: 1.6;
  margin-bottom: 0.75rem;
}

.read-more {
  display: inline-block;
  color: #6366f1;
  text-decoration: none;
  font-weight: 500;
}

.read-more:hover {
  text-decoration: underline;
}

.blog-header {
  background: linear-gradient(135deg, #6366f1 0%, #a855f7 100%);
  color: white;
  padding: 2rem;
  border-radius: 8px;
  text-align: center;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}

.blog-header h1 {
  font-size: 1.8rem;
  margin-bottom: 0.5rem;
}

.blog-header p {
  font-size: 1rem;
  opacity: 0.9;
}

.widget {
  background: white;
  border-radius: 8px;
  padding: 1.5rem;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}

.widget h3 {
  margin: 0 0 1rem;
  color: #1a1a1a;
  font-size: 1.25rem;
}

.category-widget ul {
  list-style: none;
  padding: 0;
  margin: 0;
}

.category-widget li {
  margin-bottom: 0.75rem;
}

.category-widget li:last-child {
  margin-bottom: 0;
}

.category-widget a {
  color: #4a4a4a;
  text-decoration: none;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.category-widget a:hover {
  color: #6366f1;
}

@media (max-width: 1600px) {
  .main-container {
    max-width: 100%;
  }
}

@media (max-width: 768px) {
  .home-container {
    padding: 1rem;
  }

  .main-container {
    grid-template-columns: 1fr;
    gap: 1rem;
    margin-bottom: 1rem;
  }

  .sidebar {
    min-width: 100%;
  }
}
</style> 