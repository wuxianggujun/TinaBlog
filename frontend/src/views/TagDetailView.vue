<template>
  <div class="tag-detail-container">
    <div v-if="isLoading" class="loading-container">
      <div class="spinner"></div>
      <p>加载中...</p>
    </div>

    <div v-else-if="error" class="error-container">
      <i class="fas fa-exclamation-circle"></i>
      <p>{{ errorMessage || '获取标签信息失败，请稍后再试' }}</p>
      <button @click="fetchTag" class="retry-btn">重试</button>
    </div>

    <template v-else>
      <div class="tag-header">
        <h1>
          <i class="fas fa-tag"></i>
          标签: <span class="tag-name">{{ tagInfo.name || slug }}</span>
        </h1>
        <p v-if="totalArticles > 0">该标签下共有 {{ totalArticles }} 篇文章</p>
      </div>

      <div class="tag-content">
        <div v-if="isArticlesLoading" class="loading-container">
          <div class="spinner"></div>
          <p>加载文章中...</p>
        </div>

        <div v-else-if="articlesError" class="error-container">
          <i class="fas fa-exclamation-circle"></i>
          <p>{{ articlesErrorMessage || '获取文章失败，请稍后再试' }}</p>
          <button @click="fetchArticles" class="retry-btn">重试</button>
        </div>

        <div v-else-if="articles.length === 0" class="empty-container">
          <i class="fas fa-file-alt"></i>
          <p>暂无文章</p>
        </div>

        <div v-else class="articles-list">
          <article v-for="article in articles" :key="article.id" class="article-card">
            <div class="article-header">
              <h2 class="article-title">
                <router-link :to="`/article/${article.author || article.author_username || 'anonymous'}/${article.slug}`">
                  {{ article.title }}
                </router-link>
              </h2>
              <div class="article-meta">
                <span class="article-date">
                  <i class="fas fa-calendar-alt"></i>
                  {{ formatDate(article.created_at) }}
                </span>
                <span class="article-author" v-if="article.author || article.author_username">
                  <i class="fas fa-user"></i>
                  {{ article.author || article.author_username }}
                </span>
                <router-link 
                  v-if="article.category_slug" 
                  :to="`/category/${article.category_slug}`" 
                  class="article-category"
                >
                  <i class="fas fa-folder"></i>
                  {{ article.category_name }}
                </router-link>
              </div>
            </div>
            <p class="article-summary">{{ article.summary }}</p>
            <div class="article-footer">
              <router-link :to="`/article/${article.author || article.author_username || 'anonymous'}/${article.slug}`" class="read-more">
                阅读全文 <i class="fas fa-arrow-right"></i>
              </router-link>
              
              <div class="article-tags" v-if="article.tags && article.tags.length > 0">
                <router-link 
                  v-for="tag in article.tags" 
                  :key="tag.id" 
                  :to="`/tag/${tag.slug}`"
                  class="article-tag"
                >
                  #{{ tag.name }}
                </router-link>
              </div>
            </div>
          </article>
        </div>

        <!-- 分页控件 - 只在有多页时显示 -->
        <div class="pagination" v-if="totalPages > 1">
          <button 
            @click="changePage(1)" 
            :disabled="currentPage === 1"
            class="pagination-btn"
          >
            首页
          </button>
          <button 
            @click="changePage(currentPage - 1)" 
            :disabled="currentPage === 1"
            class="pagination-btn"
          >
            上一页
          </button>
          
          <div class="pagination-pages">
            <button 
              v-for="pageNum in displayedPages" 
              :key="pageNum" 
              @click="changePage(pageNum)"
              :class="['page-btn', { active: pageNum === currentPage }]"
            >
              {{ pageNum }}
            </button>
          </div>
          
          <button 
            @click="changePage(currentPage + 1)" 
            :disabled="currentPage === totalPages"
            class="pagination-btn"
          >
            下一页
          </button>
          <button 
            @click="changePage(totalPages)" 
            :disabled="currentPage === totalPages"
            class="pagination-btn"
          >
            末页
          </button>
        </div>
      </div>
    </template>
  </div>
</template>

<script>
import axios from 'axios';

export default {
  name: 'TagDetailView',
  props: {
    slug: {
      type: String,
      required: true
    }
  },
  data() {
    return {
      tagInfo: {},
      articles: [],
      isLoading: true,
      isArticlesLoading: true,
      error: false,
      articlesError: false,
      errorMessage: '',
      articlesErrorMessage: '',
      currentPage: 1,
      pageSize: 10,
      totalArticles: 0,
      totalPages: 0
    }
  },
  created() {
    this.fetchTag();
  },
  watch: {
    slug() {
      // 当slug变化时重新获取标签信息和文章
      this.fetchTag();
    }
  },
  computed: {
    // 计算要显示的页码按钮
    displayedPages() {
      const maxVisiblePages = 5; // 最多显示5个页码按钮
      
      if (this.totalPages <= maxVisiblePages) {
        // 页数较少时显示所有页
        return Array.from({ length: this.totalPages }, (_, i) => i + 1);
      }
      
      // 计算起始和结束页
      let start = Math.max(this.currentPage - Math.floor(maxVisiblePages / 2), 1);
      let end = start + maxVisiblePages - 1;
      
      // 确保结束页不超过总页数
      if (end > this.totalPages) {
        end = this.totalPages;
        start = Math.max(end - maxVisiblePages + 1, 1);
      }
      
      return Array.from({ length: end - start + 1 }, (_, i) => start + i);
    }
  },
  methods: {
    fetchTag() {
      this.isLoading = true;
      this.error = false;
      
      // 获取标签信息
      axios.get(`/api/tags/${this.slug}`)
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.tagInfo = response.data.data.tag || {};
            // 更新页面标题
            document.title = `${this.tagInfo.name || this.slug} - 标签文章 - Tina博客`;
            // 获取标签下的文章
            this.fetchArticles();
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取标签信息失败';
          }
        })
        .catch(error => {
          console.error('获取标签信息失败:', error);
          this.error = true;
          this.errorMessage = error.response?.data?.message || '网络错误，请稍后再试';
        })
        .finally(() => {
          this.isLoading = false;
        });
    },
    
    fetchArticles() {
      this.isArticlesLoading = true;
      this.articlesError = false;
      
      // 获取标签下的文章
      axios.get(`/api/home/tag/${this.slug}`, {
        params: {
          page: this.currentPage,
          pageSize: this.pageSize
        }
      })
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.articles = response.data.data.articles || [];
            
            // 更新分页信息
            if (response.data.data.pagination) {
              this.totalArticles = response.data.data.pagination.total || 0;
              this.totalPages = response.data.data.pagination.totalPages || 0;
              this.currentPage = response.data.data.pagination.page || 1;
            } else {
              // 根据实际文章数量计算页数
              this.totalArticles = this.articles.length;
              this.totalPages = Math.ceil(this.totalArticles / this.pageSize);
            }
          } else {
            this.articlesError = true;
            this.articlesErrorMessage = response.data.message || '获取文章失败';
          }
        })
        .catch(error => {
          console.error('获取标签下文章失败:', error);
          this.articlesError = true;
          this.articlesErrorMessage = error.response?.data?.message || '网络错误，请稍后再试';
        })
        .finally(() => {
          this.isArticlesLoading = false;
        });
    },
    
    formatDate(dateString) {
      if (!dateString) return '';
      
      const options = { 
        year: 'numeric', 
        month: 'long', 
        day: 'numeric' 
      };
      
      return new Date(dateString).toLocaleDateString('zh-CN', options);
    },
    
    changePage(newPage) {
      // 确保页码在有效范围内
      if (newPage < 1 || newPage > this.totalPages) {
        return;
      }
      
      // 如果是同一页，避免重复请求
      if (newPage === this.currentPage) {
        return;
      }
      
      this.currentPage = newPage;
      this.fetchArticles();
      // 滚动到页面顶部
      window.scrollTo(0, 0);
    }
  }
}
</script>

<style scoped>
.tag-detail-container {
  max-width: 1000px;
  margin: 0 auto;
  padding: 2rem 1rem;
}

.tag-header {
  background: linear-gradient(135deg, #6366f1 0%, #a855f7 100%);
  color: white;
  text-align: center;
  padding: 3rem 2rem;
  border-radius: 8px;
  margin-bottom: 2rem;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
}

.tag-header h1 {
  font-size: 2.2rem;
  margin-bottom: 0.5rem;
}

.tag-header i {
  margin-right: 0.8rem;
}

.tag-name {
  background-color: rgba(255, 255, 255, 0.2);
  padding: 0.3rem 1rem;
  border-radius: 20px;
}

.tag-content {
  background-color: white;
  border-radius: 8px;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
  padding: 2rem;
}

.loading-container, .error-container, .empty-container {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 3rem 0;
  text-align: center;
}

.spinner {
  width: 3rem;
  height: 3rem;
  border: 3px solid rgba(0, 0, 0, 0.1);
  border-top-color: #6366f1;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
  margin-bottom: 1rem;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

.error-container i, .empty-container i {
  font-size: 3rem;
  color: #e74c3c;
  margin-bottom: 1rem;
}

.empty-container i {
  color: #6366f1;
}

.error-container p, .empty-container p {
  font-size: 1.2rem;
  color: #666;
  margin-bottom: 1rem;
}

.retry-btn {
  background-color: #6366f1;
  color: white;
  border: none;
  padding: 0.6rem 1.5rem;
  border-radius: 4px;
  font-size: 1rem;
  cursor: pointer;
  transition: background-color 0.3s;
}

.retry-btn:hover {
  background-color: #4f46e5;
}

.articles-list {
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}

.article-card {
  background-color: #f9f9f9;
  border-radius: 8px;
  padding: 1.5rem;
  transition: transform 0.3s, box-shadow 0.3s;
  border: 1px solid #eee;
}

.article-card:hover {
  transform: translateY(-3px);
  box-shadow: 0 6px 15px rgba(0, 0, 0, 0.1);
}

.article-header {
  margin-bottom: 1rem;
}

.article-title {
  font-size: 1.5rem;
  margin-bottom: 0.8rem;
}

.article-title a {
  color: #333;
  text-decoration: none;
  transition: color 0.3s;
}

.article-title a:hover {
  color: #6366f1;
}

.article-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 1rem;
  color: #666;
  font-size: 0.9rem;
}

.article-meta i {
  margin-right: 0.3rem;
}

.article-category {
  color: #6366f1;
  text-decoration: none;
}

.article-summary {
  color: #555;
  line-height: 1.6;
  margin-bottom: 1.2rem;
}

.article-footer {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 1rem;
}

.read-more {
  color: #6366f1;
  text-decoration: none;
  font-weight: 500;
  transition: color 0.3s;
  display: flex;
  align-items: center;
}

.read-more i {
  margin-left: 0.4rem;
  transition: transform 0.3s;
}

.read-more:hover i {
  transform: translateX(3px);
}

.article-tags {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
}

.article-tag {
  background-color: #eef2ff;
  color: #6366f1;
  padding: 0.2rem 0.6rem;
  border-radius: 12px;
  font-size: 0.85rem;
  text-decoration: none;
  transition: all 0.3s;
}

.article-tag:hover {
  background-color: #d1d5f0;
  transform: translateY(-2px);
}

.pagination {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-top: 2.5rem;
  gap: 0.5rem;
}

.pagination-btn, .page-btn {
  background-color: #f0f0f0;
  border: none;
  padding: 0.5rem 1rem;
  border-radius: 4px;
  cursor: pointer;
  transition: all 0.3s;
  font-size: 0.9rem;
}

.pagination-btn:hover, .page-btn:hover:not(.active) {
  background-color: #e0e0e0;
}

.pagination-btn:disabled {
  cursor: not-allowed;
  opacity: 0.5;
}

.pagination-pages {
  display: flex;
  gap: 0.3rem;
}

.page-btn {
  min-width: 2.5rem;
}

.page-btn.active {
  background-color: #6366f1;
  color: white;
}

@media (max-width: 768px) {
  .tag-header {
    padding: 2rem 1rem;
  }
  
  .tag-header h1 {
    font-size: 1.8rem;
  }
  
  .tag-content {
    padding: 1.5rem 1rem;
  }
  
  .article-meta {
    flex-direction: column;
    gap: 0.5rem;
  }
  
  .article-footer {
    flex-direction: column;
    align-items: flex-start;
  }
  
  .pagination-btn, .page-btn {
    padding: 0.4rem 0.8rem;
  }
}
</style> 