<template>
  <div class="category-container">
    <div class="category-header">
      <h1>文章分类</h1>
      <p>浏览所有分类的文章</p>
    </div>
    
    <div class="category-content">
      <div v-if="isLoading" class="loading">
        <p>正在加载分类...</p>
      </div>
      <div v-else-if="error" class="error">
        <p>{{ errorMessage }}</p>
        <button @click="fetchCategories" class="btn">重试</button>
      </div>
      <div v-else-if="categories.length === 0" class="empty-state">
        <p>暂无分类</p>
      </div>
      <div v-else class="category-grid">
        <div v-for="category in categories" :key="category.id" class="category-card">
          <router-link :to="`/category/${category.slug}`" class="category-link">
            <div class="category-card-inner">
              <h2 class="category-name">{{ category.name }}</h2>
              <div class="category-meta">
                <div class="category-count">{{ category.article_count }}篇文章</div>
                <div v-if="category.description" class="category-description">{{ category.description }}</div>
              </div>
            </div>
          </router-link>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';

export default {
  name: 'CategoryView',
  data() {
    return {
      categories: [],
      isLoading: true,
      error: false,
      errorMessage: '加载失败'
    };
  },
  created() {
    this.fetchCategories();
    // 设置页面标题
    document.title = '分类 - Tina博客';
  },
  methods: {
    fetchCategories() {
      this.isLoading = true;
      this.error = false;
      
      axios.get('/api/categories/all')
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.categories = response.data.data;
            // 按文章数量排序
            this.categories.sort((a, b) => b.article_count - a.article_count);
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取分类失败';
          }
        })
        .catch(error => {
          this.error = true;
          this.errorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('获取分类列表失败:', error);
        })
        .finally(() => {
          this.isLoading = false;
        });
    }
  }
}
</script>

<style scoped>
.category-container {
  max-width: 1200px;
  margin: 0 auto;
  padding: 2rem 1rem;
}

.category-header {
  text-align: center;
  margin-bottom: 3rem;
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

.category-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
  gap: 2rem;
}

.category-card {
  background: white;
  border-radius: 10px;
  overflow: hidden;
  box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
  transition: transform 0.3s, box-shadow 0.3s;
  height: 100%;
}

.category-card:hover {
  transform: translateY(-5px);
  box-shadow: 0 8px 25px rgba(0, 0, 0, 0.15);
}

.category-link {
  text-decoration: none;
  color: inherit;
  display: block;
  height: 100%;
}

.category-card-inner {
  padding: 2rem;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  min-height: 180px;
  height: 100%;
}

.category-name {
  font-size: 1.5rem;
  color: #333;
  margin-bottom: 1rem;
}

.category-meta {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.8rem;
}

.category-count {
  background-color: #6366f1;
  color: white;
  padding: 0.5rem 1rem;
  border-radius: 20px;
  font-size: 0.9rem;
}

.category-description {
  color: #666;
  font-size: 0.9rem;
  line-height: 1.4;
  max-width: 100%;
  overflow: hidden;
  text-overflow: ellipsis;
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
}

.loading, .error, .empty-state {
  text-align: center;
  padding: 4rem 2rem;
  background: white;
  border-radius: 10px;
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
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
}

.btn:hover {
  background-color: #4f46e5;
}

@media (max-width: 768px) {
  .category-grid {
    grid-template-columns: 1fr;
  }
  
  .category-header h1 {
    font-size: 2rem;
  }
}
</style> 