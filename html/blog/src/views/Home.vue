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
                  <router-link :to="`/article/${post.author || post.author_username || 'anonymous'}/${post.slug}`">{{ post.title }}</router-link>
                </h3>
                <div class="post-meta">
                  <span class="post-date">{{ formatDate(post.created_at) }}</span>
                  <span class="post-category" v-if="post.author">{{ post.author }}</span>
                </div>
              </div>
              <p class="post-excerpt">{{ post.summary }}</p>
              <router-link :to="`/article/${post.author || post.author_username || 'anonymous'}/${post.slug}`" class="read-more">
                阅读更多
              </router-link>
            </article>
          </div>
          <!-- 分页控件 - 只在有多页时显示 -->
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
        
        <!-- 分类区域 - 美化后 -->
        <div class="widget category-widget">
          <h3 class="widget-title">
            <i class="fas fa-folder-open"></i> 分类
            <router-link to="/categories" class="view-all">全部 <i class="fas fa-chevron-right"></i></router-link>
          </h3>
          <div v-if="categoriesLoading" class="loading-container">
            <div class="spinner"></div>
          </div>
          <div v-else-if="categoriesError" class="error-message">
            <p>加载分类失败</p>
            <button @click="fetchCategories" class="btn">重试</button>
          </div>
          <div v-else class="category-list">
            <router-link 
              v-for="category in categories.slice(0, 6)" 
              :key="category.id" 
              :to="`/category/${category.slug}`"
              class="category-item"
            >
              <div class="category-icon">
                <i :class="getCategoryIcon(category.name)"></i>
              </div>
              <div class="category-info">
                <span class="category-name">{{ category.name }}</span>
                <span class="category-count">{{ category.count || 0 }}</span>
              </div>
            </router-link>
          </div>
        </div>
        
        <!-- 标签云 - 改为热门标签 -->
        <div class="widget tag-widget">
          <h3 class="widget-title">
            <i class="fas fa-tags"></i> 热门标签
            <router-link to="/tags" class="view-all">全部 <i class="fas fa-chevron-right"></i></router-link>
          </h3>
          <div v-if="tagsLoading" class="loading-container">
            <div class="spinner"></div>
          </div>
          <div v-else-if="tagsError" class="error-message">
            <p>加载标签失败</p>
            <button @click="fetchAllTags" class="btn">重试</button>
          </div>
          <div v-else-if="tags.length === 0" class="empty-message">
            <p>暂无标签</p>
          </div>
          <div v-else class="tag-cloud">
            <router-link 
              v-for="tag in tags.slice(0, 20)" 
              :key="tag.id" 
              :to="`/tag/${tag.slug}`"
              class="tag-item"
              :style="{ fontSize: calculateTagSize(tag.count, tags) }"
            >
              #{{ tag.name }}
            </router-link>
          </div>
        </div>
        
        <!-- 用户标签组件 - 仅已登录用户显示 -->
        <div v-if="isLoggedIn" class="widget user-tag-widget">
          <h3 class="widget-title">
            <i class="fas fa-user-tag"></i> 我的标签
          </h3>
          <div v-if="userTagsLoading" class="loading-container">
            <div class="spinner"></div>
          </div>
          <div v-else-if="userTagsError" class="error-message">
            <p>加载标签失败</p>
            <button @click="fetchUserTags" class="btn">重试</button>
          </div>
          <div v-else-if="userTags.length === 0" class="empty-message">
            <p>暂无个人标签</p>
            <p class="hint">发布文章时添加标签，它们将显示在这里</p>
          </div>
          <div v-else class="tag-cloud user-tags">
            <router-link 
              v-for="tag in userTags" 
              :key="tag.id" 
              :to="`/tag/${tag.slug}`"
              class="tag-item user-tag"
              :style="{ fontSize: calculateTagSize(tag.count, userTags) }"
            >
              #{{ tag.name }}
            </router-link>
          </div>
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
      tags: [],
      userTags: [],
      isLoading: true,
      categoriesLoading: true,
      tagsLoading: true,
      userTagsLoading: false,
      error: false,
      categoriesError: false,
      tagsError: false,
      userTagsError: false,
      errorMessage: '加载失败',
      currentPage: 1,
      pageSize: 10,
      totalPosts: 0,
      totalPages: 0,
      isLoggedIn: false
    }
  },
  created() {
    // 不再默认设置页数，而是从服务器获取正确的页数
    this.isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
    this.fetchArticles();
    this.fetchCategories();
    this.fetchAllTags();
    
    // 添加事件监听，当用户登录状态改变时刷新标签
    window.addEventListener('storage', this.handleStorageChange);
    
    // 如果已登录，获取用户标签
    if (this.isLoggedIn) {
      this.fetchUserTags();
    }
  },
  beforeUnmount() {
    // 移除事件监听器
    window.removeEventListener('storage', this.handleStorageChange);
  },
  methods: {
    // 添加监听localStorage的事件处理
    handleStorageChange(event) {
      if (event.key === 'isLoggedIn') {
        const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
        this.isLoggedIn = isLoggedIn;
        
        if (isLoggedIn) {
          this.fetchUserTags();
        } else {
          // 用户登出时清空标签
          this.userTags = [];
        }
      }
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
    
    fetchArticles() {
      this.isLoading = true;
      this.error = false;
      
      console.log('请求文章页面:', this.currentPage, '每页数量:', this.pageSize);
      
      axios.get('/api/home/recent', { 
        params: {
          page: this.currentPage,
          pageSize: this.pageSize
        }
      })
        .then(response => {
          console.log('获取文章响应:', response.data);
          if (response.data.code === 0 && response.data.data) {
            this.posts = response.data.data.articles || [];
            
            // 更新分页信息 - 使用服务器返回的准确数据
            if (response.data.data.pagination) {
              this.totalPosts = response.data.data.pagination.total || 0;
              this.totalPages = response.data.data.pagination.totalPages || 0;
              this.currentPage = response.data.data.pagination.page || 1;
              
              // 确保currentPage不超过实际页数
              if (this.currentPage > this.totalPages && this.totalPages > 0) {
                this.currentPage = this.totalPages;
              }
              
              console.log('分页信息更新:', {
                currentPage: this.currentPage,
                totalPages: this.totalPages,
                totalPosts: this.totalPosts
              });
            } else {
              console.warn('响应中缺少分页信息');
              // 根据实际文章数量计算页数
              this.totalPages = Math.ceil(this.posts.length / this.pageSize);
              console.log('根据文章数量计算分页:', this.totalPages);
            }
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取文章失败';
            console.error('文章响应错误:', this.errorMessage);
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
    
    fetchAllTags() {
      this.tagsLoading = true;
      this.tagsError = false;
      
      axios.get('/api/tags')
        .then(response => {
          console.log('标签响应:', response.data);
          if (response.data.code === 0 && response.data.data) {
            this.tags = response.data.data.tags || [];
            console.log('获取到标签:', this.tags.length);
          } else {
            this.tagsError = true;
            console.error('获取标签响应错误:', response.data.message);
          }
        })
        .catch(error => {
          console.error('获取标签失败:', error);
          this.tagsError = true;
        })
        .finally(() => {
          this.tagsLoading = false;
        });
    },
    
    fetchUserTags() {
      if (!this.isLoggedIn) return;
      
      this.userTagsLoading = true;
      this.userTagsError = false;
      
      // 获取token
      const token = localStorage.getItem('token');
      if (!token) {
        this.userTagsLoading = false;
        return;
      }
      
      // 使用正确的API端点并确保设置了Authorization头
      axios.get('/api/tags/user', {
        headers: {
          'Authorization': `Bearer ${token}`
        }
      })
        .then(response => {
          console.log('用户标签响应:', response.data);
          if (response.data.code === 0 && response.data.data) {
            this.userTags = response.data.data.tags || [];
            console.log('获取到用户标签:', this.userTags.length);
          } else {
            this.userTagsError = true;
            console.error('获取用户标签响应错误:', response.data.message);
          }
        })
        .catch(error => {
          console.error('获取用户标签失败:', error);
          this.userTagsError = true;
        })
        .finally(() => {
          this.userTagsLoading = false;
        });
    },
    
    // 根据标签使用次数计算标签云中的字体大小
    calculateTagSize(count, tagCollection) {
      // 基础字体大小12px，最大18px
      const minSize = 12;
      const maxSize = 18;
      const minCount = 1;
      
      // 获取所有标签中最高的count值
      const maxCount = Math.max(...tagCollection.map(tag => tag.count || 0), minCount);
      
      // 如果所有标签都是相同的count，返回默认尺寸
      if (maxCount === minCount) return `${minSize}px`;
      
      // 计算大小比例
      let size = minSize + (count - minCount) * (maxSize - minSize) / (maxCount - minCount);
      // 确保在范围内
      size = Math.max(minSize, Math.min(maxSize, size));
      return `${size}px`;
    },
    
    // 根据分类名称获取对应的图标class
    getCategoryIcon(categoryName) {
      const icons = {
        '技术': 'fas fa-code',
        '编程': 'fas fa-laptop-code',
        '前端': 'fab fa-js',
        '后端': 'fas fa-server',
        '数据库': 'fas fa-database',
        '人工智能': 'fas fa-robot',
        '机器学习': 'fas fa-brain',
        '深度学习': 'fas fa-network-wired',
        '算法': 'fas fa-calculator',
        '区块链': 'fab fa-bitcoin',
        '云计算': 'fas fa-cloud',
        '运维': 'fas fa-cogs',
        '网络': 'fas fa-network-wired',
        '安全': 'fas fa-shield-alt',
        '开源': 'fab fa-github',
        '工具': 'fas fa-tools',
        '生活': 'fas fa-coffee',
        '随笔': 'fas fa-pen',
        '读书': 'fas fa-book',
        '影评': 'fas fa-film',
        '旅行': 'fas fa-plane',
        '摄影': 'fas fa-camera',
        '音乐': 'fas fa-music',
        '游戏': 'fas fa-gamepad',
        '设计': 'fas fa-palette',
        '其他': 'fas fa-folder'
      };
      
      return icons[categoryName] || 'fas fa-folder';
    },
    
    changePage(newPage) {
      console.log('切换到页面:', newPage, '当前页:', this.currentPage);
      // 确保页码在有效范围内
      if (newPage < 1 || newPage > this.totalPages) {
        console.warn('无效页码:', newPage);
        return;
      }
      
      // 如果是同一页，避免重复请求
      if (newPage === this.currentPage) {
        console.log('已经在页面', newPage);
        return;
      }
      
      this.currentPage = newPage;
      this.fetchArticles();
      // 滚动到页面顶部
      window.scrollTo(0, 0);
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
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
  transition: transform 0.3s, box-shadow 0.3s;
}

.widget:hover {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
}

.widget-title {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 1.2rem;
  padding-bottom: 0.8rem;
  border-bottom: 1px solid #eee;
  font-size: 1.1rem;
  font-weight: 600;
}

.widget-title i {
  margin-right: 0.5rem;
  color: #6366f1;
}

.view-all {
  font-size: 0.85rem;
  font-weight: normal;
  color: #888;
  display: flex;
  align-items: center;
  text-decoration: none;
  transition: color 0.2s;
}

.view-all:hover {
  color: #6366f1;
}

.view-all i {
  font-size: 0.75rem;
  margin-left: 0.3rem;
}

.category-list {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 0.75rem;
}

.category-item {
  display: flex;
  align-items: center;
  text-decoration: none;
  color: inherit;
  padding: 0.6rem;
  border-radius: 6px;
  background-color: #f8f9fa;
  transition: all 0.3s;
}

.category-item:hover {
  background-color: #eef2ff;
  transform: translateY(-2px);
}

.category-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 2.5rem;
  height: 2.5rem;
  background-color: #6366f1;
  border-radius: 50%;
  margin-right: 0.75rem;
  color: white;
  flex-shrink: 0;
}

.category-info {
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.category-name {
  font-weight: 500;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.category-count {
  font-size: 0.75rem;
  color: #888;
}

.tag-cloud {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
}

.tag-item {
  display: inline-block;
  padding: 0.3rem 0.8rem;
  background-color: #f0f2f5;
  border-radius: 20px;
  color: #555;
  text-decoration: none;
  transition: all 0.3s;
}

.tag-item:hover {
  background-color: #e6f7ff;
  color: #1677ff;
  transform: translateY(-2px);
}

.user-tags {
  margin-top: 0.5rem;
}

.user-tag {
  background-color: #eef2ff;
}

.user-tag:hover {
  background-color: #d1d5f0;
  color: #4f46e5;
}

.loading-container {
  display: flex;
  justify-content: center;
  padding: 1rem 0;
}

.spinner {
  width: 2rem;
  height: 2rem;
  border: 2px solid rgba(0, 0, 0, 0.1);
  border-top-color: #6366f1;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

.error-message {
  color: #e74c3c;
  text-align: center;
  padding: 1rem 0;
}

.empty-message {
  color: #666;
  text-align: center;
  padding: 1rem 0;
}

.hint {
  font-size: 0.85rem;
  color: #999;
  margin-top: 0.5rem;
}

.pagination {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-top: 2rem;
  gap: 0.5rem;
  flex-wrap: wrap;
  padding: 1rem;
  background-color: #f8f9fa;
  border-radius: 8px;
  box-shadow: 0 1px 3px rgba(0,0,0,0.1);
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
  border-radius: 4px;
  font-weight: 500;
}

.page-btn.active {
  background-color: #4f46e5;
  font-weight: bold;
  transform: scale(1.1);
  box-shadow: 0 1px 4px rgba(0,0,0,0.2);
}

.pagination-btn {
  padding: 0.5rem 0.75rem;
  font-weight: 500;
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

  .category-list {
    grid-template-columns: 1fr;
  }
}
</style> 