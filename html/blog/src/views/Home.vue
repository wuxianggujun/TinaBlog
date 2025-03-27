<template>
  <div class="home-container">
    <div class="main-container">
      <!-- 左侧文章列表 -->
      <div class="content">
        <div class="content-card">
          <h2 class="section-title">最新文章</h2>
          <div v-if="isLoading" class="loading">
            <p>加载中...</p>
          </div>
          <div v-else-if="error" class="error">
            <p>{{ errorMessage }}</p>
            <button @click="fetchArticles" class="read-more">重试</button>
          </div>
          <div v-else class="post-list">
            <article v-for="post in posts" :key="post.id" class="post-card">
              <div class="post-header">
                <h3 class="post-title">
                  <router-link :to="`/article/${post.author_username || 'author'}/${post.slug}`">{{ post.title }}</router-link>
                </h3>
                <div class="post-meta">
                  <span class="post-date">{{ formatDate(post.created_at) }}</span>
                  <span class="post-category" v-if="post.author">{{ post.author }}</span>
                </div>
              </div>
              <p class="post-excerpt">{{ post.summary }}</p>
              <router-link :to="`/article/${post.author_username || 'author'}/${post.slug}`" class="read-more">
                阅读更多
              </router-link>
            </article>
          </div>
          <!-- 分页控件 -->
          <div class="pagination" v-if="totalPages > 1">
            <button 
              @click="changePage(currentPage - 1)" 
              :disabled="currentPage === 1"
              class="btn"
            >
              上一页
            </button>
            
            <span class="page-info">{{ currentPage }} / {{ totalPages }}</span>
            
            <button 
              @click="changePage(currentPage + 1)" 
              :disabled="currentPage === totalPages"
              class="btn"
            >
              下一页
            </button>
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
          <div v-if="categoriesLoading" class="loading">
            <p>加载中...</p>
          </div>
          <div v-else-if="categoriesError" class="error">
            <p>加载分类失败</p>
            <button @click="fetchCategories" class="btn">重试</button>
          </div>
          <ul v-else>
            <li v-for="category in categories" :key="category.id">
              <router-link :to="'/category/' + category.slug">
                {{ category.name }} ({{ category.count }})
              </router-link>
            </li>
          </ul>
        </div>
      </aside>
    </div>
  </div>
</template>

<script>
import axios from 'axios';

export default {
  name: 'Home',
  data() {
    return {
      posts: [],
      categories: [],
      isLoading: true,
      categoriesLoading: true,
      error: false,
      categoriesError: false,
      errorMessage: '加载失败',
      currentPage: 1,
      pageSize: 10,
      totalPosts: 0,
      totalPages: 0
    }
  },
  created() {
    this.fetchArticles();
    this.fetchCategories();
  },
  methods: {
    formatDate(dateString) {
      if (!dateString) return '';
      
      const options = { 
        year: 'numeric', 
        month: 'long', 
        day: 'numeric' 
      };
      
      return new Date(dateString).toLocaleDateString('zh-CN', options);
    },
    
    fetchArticles() {
      this.isLoading = true;
      this.error = false;
      
      axios.get('/api/home/recent', { 
        params: {
          page: this.currentPage,
          pageSize: this.pageSize
        }
      })
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.posts = response.data.data.articles || [];
            
            // 更新分页信息
            if (response.data.data.pagination) {
              this.totalPosts = response.data.data.pagination.total || 0;
              this.totalPages = response.data.data.pagination.totalPages || 0;
              this.currentPage = response.data.data.pagination.page || 1;
            }
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取文章失败';
          }
        })
        .catch(error => {
          this.error = true;
          this.errorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('获取文章列表失败:', error);
        })
        .finally(() => {
          this.isLoading = false;
        });
    },
    
    fetchCategories() {
      this.categoriesLoading = true;
      this.categoriesError = false;
      
      axios.get('/api/categories')
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.categories = response.data.data.categories || [];
          } else {
            this.categoriesError = true;
          }
        })
        .catch(error => {
          this.categoriesError = true;
          console.error('获取分类失败:', error);
        })
        .finally(() => {
          this.categoriesLoading = false;
        });
    },
    
    changePage(newPage) {
      this.currentPage = newPage;
      this.fetchArticles();
      // 滚动到页面顶部
      window.scrollTo(0, 0);
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

.pagination {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-top: 2rem;
  gap: 1rem;
}

.btn {
  padding: 0.5rem 1rem;
  background-color: #6366f1;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-weight: 500;
}

.btn:disabled {
  background-color: #c4c4c4;
  cursor: not-allowed;
}

.page-info {
  font-size: 0.9rem;
  color: #666;
}

.loading, .error {
  text-align: center;
  padding: 2rem;
  color: #666;
}

.error button {
  margin-top: 1rem;
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