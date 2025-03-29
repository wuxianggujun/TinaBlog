<template>
  <div class="category-detail-container">
    <div class="category-header">
      <h1>{{ categoryName || '分类' }}</h1>
      <p v-if="isLoading">正在加载分类信息...</p>
      <p v-else-if="!error">该分类下的所有文章</p>
    </div>
    
    <div class="content-card">
      <div v-if="isLoading" class="loading">
        <p>正在加载文章...</p>
      </div>
      <div v-else-if="error" class="error">
        <p>{{ errorMessage }}</p>
        <button @click="fetchCategoryArticles" class="btn">重试</button>
      </div>
      <div v-else-if="posts.length === 0" class="empty-state">
        <p>该分类下暂无文章</p>
        <router-link to="/categories" class="btn">返回分类页</router-link>
      </div>
      <div v-else class="post-list">
        <article v-for="post in posts" :key="post.id" class="post-card">
          <div class="post-header">
            <h3 class="post-title">
              <router-link :to="`/article/${post.author || 'anonymous'}/${post.slug}`">{{ post.title }}</router-link>
            </h3>
            <div class="post-meta">
              <span class="post-date">{{ formatDate(post.created_at) }}</span>
              <span class="post-author" v-if="post.author">{{ post.author }}</span>
            </div>
          </div>
          <p class="post-excerpt">{{ post.summary }}</p>
          <router-link :to="`/article/${post.author || 'anonymous'}/${post.slug}`" class="read-more">
            阅读更多
          </router-link>
        </article>
      </div>
      
      <!-- 分页控件 -->
      <div class="pagination" v-if="totalPages > 1">
        <button 
          @click="changePage(1)" 
          :disabled="currentPage === 1"
          class="btn pagination-btn"
        >
          首页
        </button>
        <button 
          @click="changePage(currentPage - 1)" 
          :disabled="currentPage === 1"
          class="btn pagination-btn"
        >
          上一页
        </button>
        
        <div class="pagination-pages">
          <button 
            v-for="pageNum in displayedPages" 
            :key="pageNum" 
            @click="changePage(pageNum)"
            :class="['btn', 'page-btn', { active: pageNum === currentPage }]"
          >
            {{ pageNum }}
          </button>
        </div>
        
        <button 
          @click="changePage(currentPage + 1)" 
          :disabled="currentPage === totalPages"
          class="btn pagination-btn"
        >
          下一页
        </button>
        <button 
          @click="changePage(totalPages)" 
          :disabled="currentPage === totalPages"
          class="btn pagination-btn"
        >
          末页
        </button>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';

export default {
  name: 'CategoryDetailView',
  props: {
    slug: {
      type: String,
      required: true
    }
  },
  data() {
    return {
      categoryName: '',
      posts: [],
      isLoading: true,
      error: false,
      errorMessage: '加载失败',
      currentPage: 1,
      pageSize: 10,
      totalPosts: 0,
      totalPages: 0
    };
  },
  created() {
    this.fetchCategoryArticles();
  },
  watch: {
    // 当路由参数变化时重新获取数据
    slug() {
      this.fetchCategoryArticles();
    }
  },
  methods: {
    formatDate(dateString) {
      if (!dateString) return '';
      
      const options = { year: 'numeric', month: 'long', day: 'numeric' };
      return new Date(dateString).toLocaleDateString('zh-CN', options);
    },
    
    fetchCategoryArticles() {
      this.isLoading = true;
      this.error = false;
      
      axios.get(`/api/category/${this.slug}`, {
        params: {
          page: this.currentPage,
          pageSize: this.pageSize
        }
      })
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.posts = response.data.data.articles || [];
            
            // 设置分类名称
            if (response.data.data.categoryName) {
              this.categoryName = response.data.data.categoryName;
              // 更新页面标题
              document.title = `${this.categoryName} - 分类 - Tina博客`;
            }
            
            // 更新分页信息
            if (response.data.data.pagination) {
              this.totalPosts = response.data.data.pagination.total || 0;
              this.totalPages = response.data.data.pagination.totalPages || 0;
              this.currentPage = response.data.data.pagination.page || 1;
            }
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取分类文章失败';
          }
        })
        .catch(error => {
          this.error = true;
          this.errorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('获取分类文章失败:', error);
        })
        .finally(() => {
          this.isLoading = false;
        });
    },
    
    changePage(newPage) {
      if (newPage < 1 || newPage > this.totalPages) return;
      
      if (newPage === this.currentPage) return;
      
      this.currentPage = newPage;
      this.fetchCategoryArticles();
      // 滚动到页面顶部
      window.scrollTo(0, 0);
    }
  },
  computed: {
    // 计算要显示的页码按钮
    displayedPages() {
      const maxVisiblePages = 5;
      
      if (this.totalPages <= maxVisiblePages) {
        return Array.from({ length: this.totalPages }, (_, i) => i + 1);
      }
      
      let start = Math.max(this.currentPage - Math.floor(maxVisiblePages / 2), 1);
      let end = start + maxVisiblePages - 1;
      
      if (end > this.totalPages) {
        end = this.totalPages;
        start = Math.max(end - maxVisiblePages + 1, 1);
      }
      
      return Array.from({ length: end - start + 1 }, (_, i) => start + i);
    }
  }
}
</script>

<style scoped>
.category-detail-container {
  max-width: 1000px;
  margin: 0 auto;
  padding: 2rem 1rem;
}

.category-header {
  text-align: center;
  margin-bottom: 2rem;
}

.category-header h1 {
  font-size: 2.5rem;
  color: #333;
  margin-bottom: 0.5rem;
}

.category-header p {
  font-size: 1.2rem;
  color: #666;
}

.content-card {
  background: white;
  border-radius: 10px;
  padding: 2rem;
  box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
}

.post-list {
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}

.post-card {
  padding: 1.5rem;
  border-bottom: 1px solid #eee;
  transition: transform 0.2s;
}

.post-card:hover {
  transform: translateY(-3px);
}

.post-card:last-child {
  border-bottom: none;
}

.post-header {
  margin-bottom: 1rem;
}

.post-title {
  font-size: 1.5rem;
  margin: 0 0 0.5rem;
}

.post-title a {
  color: #333;
  text-decoration: none;
  transition: color 0.2s;
}

.post-title a:hover {
  color: #6366f1;
}

.post-meta {
  display: flex;
  gap: 1rem;
  font-size: 0.9rem;
  color: #666;
}

.post-excerpt {
  color: #555;
  line-height: 1.6;
  margin-bottom: 1rem;
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

.loading, .error, .empty-state {
  text-align: center;
  padding: 3rem 0;
  color: #666;
}

.btn {
  padding: 0.5rem 1.5rem;
  background-color: #6366f1;
  color: white;
  border: none;
  border-radius: 5px;
  cursor: pointer;
  font-weight: 500;
  margin-top: 1rem;
  text-decoration: none;
}

.btn:hover {
  background-color: #4f46e5;
}

.pagination {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-top: 2rem;
  gap: 0.5rem;
  flex-wrap: wrap;
}

.pagination-pages {
  display: flex;
  gap: 0.25rem;
}

.page-btn {
  min-width: 2.5rem;
  height: 2.5rem;
  padding: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-top: 0;
}

.page-btn.active {
  background-color: #4f46e5;
  font-weight: bold;
}

.pagination-btn {
  margin-top: 0;
}

@media (max-width: 768px) {
  .category-header h1 {
    font-size: 2rem;
  }
  
  .content-card {
    padding: 1.5rem;
  }
}
</style> 