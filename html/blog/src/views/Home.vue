<template>
  <div class="home-container">
    <div class="container">
      <!-- 主内容区 -->
      <div class="main-content">
        <h1>最新文章</h1>
        
        <!-- 加载中状态 -->
        <div v-if="isLoading" class="loading-container">
          <div class="loading-spinner"></div>
          <p>正在加载文章...</p>
        </div>
        
        <!-- 加载错误状态 -->
        <div v-else-if="error" class="error-message">
          <p>{{ errorMessage }}</p>
          <button @click="fetchPosts" class="btn-retry">重试</button>
        </div>
        
        <!-- 没有文章状态 -->
        <div v-else-if="posts.length === 0" class="no-posts">
          <p>暂无文章</p>
        </div>
        
        <!-- 文章列表 -->
        <div v-else class="posts-container">
          <div v-for="post in posts" :key="post.id" class="post-card">
            <h2 class="post-title">
              <router-link :to="'/article/' + post.slug">{{ post.title }}</router-link>
            </h2>
            
            <div class="post-meta">
              <span class="post-author">作者: {{ post.author }}</span>
              <span class="post-date">发布于: {{ formatDate(post.created_at) }}</span>
            </div>
            
            <div class="post-summary">
              {{ post.summary || post.preview }}
            </div>
            
            <div class="post-actions">
              <router-link :to="'/article/' + post.slug" class="read-more">
                阅读全文
              </router-link>
            </div>
          </div>
          
          <!-- 分页控件 -->
          <div class="pagination" v-if="totalPages > 1">
            <button 
              @click="changePage(currentPage - 1)" 
              :disabled="currentPage === 1"
              class="page-btn"
            >
              上一页
            </button>
            
            <span class="page-info">{{ currentPage }} / {{ totalPages }}</span>
            
            <button 
              @click="changePage(currentPage + 1)" 
              :disabled="currentPage === totalPages"
              class="page-btn"
            >
              下一页
            </button>
          </div>
        </div>
      </div>
      
      <!-- 侧边栏 -->
      <div class="sidebar">
        <!-- 分类列表 -->
        <div class="sidebar-section categories-section">
          <h3>分类</h3>
          <div v-if="categoriesLoading" class="sidebar-loading">
            <div class="sidebar-spinner"></div>
          </div>
          <div v-else-if="categoriesError" class="sidebar-error">
            加载分类失败
            <button @click="fetchCategories" class="btn-retry-small">重试</button>
          </div>
          <ul v-else class="categories-list">
            <li v-for="category in categories" :key="category.id" class="category-item">
              <router-link 
                :to="{ path: '/', query: { category: category.slug } }"
                :class="{ active: activeCategory === category.slug }"
              >
                {{ category.name }} ({{ category.count }})
              </router-link>
            </li>
          </ul>
        </div>
        
        <!-- 标签列表 -->
        <div class="sidebar-section tags-section">
          <h3>标签</h3>
          <div v-if="tagsLoading" class="sidebar-loading">
            <div class="sidebar-spinner"></div>
          </div>
          <div v-else-if="tagsError" class="sidebar-error">
            加载标签失败
            <button @click="fetchTags" class="btn-retry-small">重试</button>
          </div>
          <div v-else class="tags-cloud">
            <router-link 
              v-for="tag in tags" 
              :key="tag.id"
              :to="{ path: '/', query: { tag: tag.slug } }"
              :class="{ 'tag-link': true, active: activeTag === tag.slug }"
              :style="{ fontSize: getTagFontSize(tag.count) }"
            >
              {{ tag.name }}
            </router-link>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';

export default {
  name: 'HomeView',
  
  data() {
    return {
      posts: [],
      categories: [],
      tags: [],
      isLoading: true,
      categoriesLoading: true,
      tagsLoading: true,
      error: false,
      categoriesError: false,
      tagsError: false,
      errorMessage: '',
      currentPage: 1,
      pageSize: 10,
      totalPosts: 0,
      totalPages: 0
    };
  },

  computed: {
    activeCategory() {
      return this.$route.query.category || '';
    },
    
    activeTag() {
      return this.$route.query.tag || '';
    }
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
    
    fetchPosts() {
      this.isLoading = true;
      this.error = false;
      
      // 构建查询参数
      const params = {
        page: this.currentPage,
        pageSize: this.pageSize
      };
      
      // 添加分类和标签过滤
      if (this.activeCategory) {
        params.category = this.activeCategory;
      }
      
      if (this.activeTag) {
        params.tag = this.activeTag;
      }
      
      axios.get('/api/articles', { params })
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
          this.errorMessage = error.response?.data?.message || '获取文章时出错';
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
    
    fetchTags() {
      this.tagsLoading = true;
      this.tagsError = false;
      
      axios.get('/api/tags')
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.tags = response.data.data.tags || [];
          } else {
            this.tagsError = true;
          }
        })
        .catch(error => {
          this.tagsError = true;
          console.error('获取标签失败:', error);
        })
        .finally(() => {
          this.tagsLoading = false;
        });
    },
    
    changePage(page) {
      if (page < 1 || page > this.totalPages) return;
      
      this.currentPage = page;
      
      // 更新查询参数但保留当前的分类和标签过滤
      const query = { ...this.$route.query, page };
      
      // 使用 router 来更新 URL 而不重新加载页面
      this.$router.push({ 
        path: this.$route.path, 
        query 
      });
      
      // 获取新页面的文章
      this.fetchPosts();
      
      // 滚动到页面顶部
      window.scrollTo(0, 0);
    },
    
    getTagFontSize(count) {
      // 根据文章数量计算标签字体大小，最小 0.9rem，最大 1.5rem
      const min = 0.9;
      const max = 1.5;
      
      // 找出最大的 count 值
      const maxCount = Math.max(...this.tags.map(tag => tag.count));
      
      if (maxCount <= 1) return `${min}rem`;
      
      // 计算字体大小
      const size = min + ((count - 1) / (maxCount - 1)) * (max - min);
      return `${size}rem`;
    }
  },
  
  mounted() {
    // 读取URL中的页码
    const pageParam = this.$route.query.page;
    if (pageParam) {
      this.currentPage = parseInt(pageParam, 10) || 1;
    }
    
    // 获取数据
    this.fetchPosts();
    this.fetchCategories();
    this.fetchTags();
  },
  
  watch: {
    // 监听路由变化，当分类或标签改变时刷新文章列表
    '$route.query'(newQuery) {
      // 如果分类或标签改变，重置到第一页
      if (newQuery.category !== this.$route.query.category || 
          newQuery.tag !== this.$route.query.tag) {
        this.currentPage = 1;
      }
      
      this.fetchPosts();
    }
  }
};
</script>

<style scoped>
.home-container {
  max-width: 1200px;
  margin: 0 auto;
  padding: 20px;
}

.container {
  display: flex;
  flex-direction: row;
  gap: 30px;
}

@media (max-width: 768px) {
  .container {
    flex-direction: column;
  }
}

.main-content {
  flex: 1;
}

.sidebar {
  width: 300px;
}

h1 {
  margin-bottom: 25px;
  font-size: 1.8rem;
  color: #333;
}

.loading-container, .error-message, .no-posts {
  text-align: center;
  padding: 40px 0;
}

.loading-spinner, .sidebar-spinner {
  border: 4px solid rgba(0, 0, 0, 0.1);
  border-radius: 50%;
  border-top: 4px solid #3498db;
  width: 30px;
  height: 30px;
  margin: 0 auto 20px;
  animation: spin 1s linear infinite;
}

.sidebar-spinner {
  width: 20px;
  height: 20px;
  margin-bottom: 10px;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

.posts-container {
  display: flex;
  flex-direction: column;
  gap: 30px;
}

.post-card {
  border: 1px solid #eee;
  border-radius: 8px;
  padding: 20px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.05);
  transition: transform 0.2s, box-shadow 0.2s;
}

.post-card:hover {
  transform: translateY(-3px);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
}

.post-title {
  margin-top: 0;
  margin-bottom: 10px;
  font-size: 1.5rem;
}

.post-title a {
  color: #333;
  text-decoration: none;
  transition: color 0.2s;
}

.post-title a:hover {
  color: #3498db;
}

.post-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 15px;
  margin-bottom: 15px;
  font-size: 0.9rem;
  color: #666;
}

.post-summary {
  margin-bottom: 20px;
  line-height: 1.5;
  color: #444;
}

.post-actions {
  text-align: right;
}

.read-more {
  display: inline-block;
  padding: 8px 16px;
  background-color: #3498db;
  color: white;
  text-decoration: none;
  border-radius: 4px;
  transition: background-color 0.2s;
}

.read-more:hover {
  background-color: #2980b9;
}

.pagination {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-top: 30px;
  gap: 15px;
}

.page-btn {
  padding: 8px 16px;
  background-color: #f8f9fa;
  border: 1px solid #dee2e6;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.page-btn:hover:not(:disabled) {
  background-color: #e9ecef;
}

.page-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.page-info {
  font-size: 0.9rem;
  color: #666;
}

.sidebar-section {
  margin-bottom: 30px;
  padding: 20px;
  background-color: #f8f9fa;
  border-radius: 8px;
}

.sidebar-section h3 {
  margin-top: 0;
  margin-bottom: 15px;
  font-size: 1.2rem;
  color: #333;
  border-bottom: 1px solid #dee2e6;
  padding-bottom: 10px;
}

.categories-list {
  list-style: none;
  padding: 0;
  margin: 0;
}

.category-item {
  margin-bottom: 8px;
}

.category-item a {
  display: block;
  padding: 5px 0;
  color: #333;
  text-decoration: none;
  transition: color 0.2s;
}

.category-item a:hover, .category-item a.active {
  color: #3498db;
}

.tags-cloud {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
}

.tag-link {
  color: #3498db;
  text-decoration: none;
  padding: 3px 8px;
  background-color: #e9f5fe;
  border-radius: 4px;
  transition: background-color 0.2s;
}

.tag-link:hover, .tag-link.active {
  background-color: #d0e8f9;
}

.sidebar-loading, .sidebar-error {
  text-align: center;
  padding: 10px 0;
  font-size: 0.9rem;
  color: #666;
}

.btn-retry, .btn-retry-small {
  display: inline-block;
  margin-top: 10px;
  padding: 8px 16px;
  background-color: #3498db;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.btn-retry-small {
  font-size: 0.8rem;
  padding: 4px 8px;
  margin-top: 5px;
}

.btn-retry:hover, .btn-retry-small:hover {
  background-color: #2980b9;
}
</style> 