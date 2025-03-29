<template>
  <div class="archive-container">
    <div class="archive-header">
      <h1>文章归档</h1>
      <p>按照时间查看所有文章</p>
    </div>
    
    <div class="content-card">
      <div v-if="isLoading" class="loading">
        <p>正在加载归档数据...</p>
      </div>
      <div v-else-if="error" class="error">
        <p>{{ errorMessage }}</p>
        <button @click="fetchArchives" class="btn">重试</button>
      </div>
      <div v-else-if="!archives || Object.keys(archives).length === 0" class="empty-state">
        <p>暂无归档文章</p>
        <router-link to="/" class="btn">返回首页</router-link>
      </div>
      <div v-else class="archive-timeline">
        <div v-for="(year, yearKey) in archives" :key="yearKey" class="archive-year">
          <div class="year-header">
            <h2>{{ yearKey }}年</h2>
            <span class="article-count">{{ countArticlesInYear(year) }}篇文章</span>
          </div>
          
          <div class="months-container">
            <div v-for="(month, monthKey) in year" :key="`${yearKey}-${monthKey}`" class="archive-month">
              <h3 class="month-header">{{ monthKey }}月</h3>
              
              <ul class="article-list">
                <li v-for="article in month" :key="article.id" class="article-item">
                  <div class="article-date">{{ formatDay(article.created_at) }}</div>
                  <div class="article-title">
                    <router-link :to="`/article/${article.author || 'anonymous'}/${article.slug}`">
                      {{ article.title }}
                    </router-link>
                  </div>
                </li>
              </ul>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';

export default {
  name: 'ArchiveView',
  data() {
    return {
      archives: null,
      isLoading: true,
      error: false,
      errorMessage: '加载失败'
    };
  },
  created() {
    this.fetchArchives();
  },
  methods: {
    // 格式化日期，仅返回日
    formatDay(dateString) {
      if (!dateString) return '';
      
      const date = new Date(dateString);
      return date.getDate() + '日';
    },
    
    // 获取归档数据
    fetchArchives() {
      this.isLoading = true;
      this.error = false;
      
      axios.get('/api/archives')
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.archives = this.processArchiveData(response.data.data);
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取归档数据失败';
          }
        })
        .catch(error => {
          this.error = true;
          this.errorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('获取归档数据失败:', error);
        })
        .finally(() => {
          this.isLoading = false;
        });
    },
    
    // 处理归档数据，按年月组织
    processArchiveData(articles) {
      if (!articles || !articles.length) return {};
      
      const archive = {};
      
      articles.forEach(article => {
        if (!article.created_at) return;
        
        const date = new Date(article.created_at);
        const year = date.getFullYear();
        const month = date.getMonth() + 1; // 月份从0开始，需要+1
        
        if (!archive[year]) {
          archive[year] = {};
        }
        
        if (!archive[year][month]) {
          archive[year][month] = [];
        }
        
        archive[year][month].push(article);
      });
      
      // 对年份进行排序（降序）
      const orderedArchive = {};
      Object.keys(archive)
        .sort((a, b) => b - a)
        .forEach(year => {
          orderedArchive[year] = {};
          
          // 对月份进行排序（降序）
          Object.keys(archive[year])
            .sort((a, b) => b - a)
            .forEach(month => {
              orderedArchive[year][month] = archive[year][month];
            });
        });
      
      return orderedArchive;
    },
    
    // 计算某年的文章总数
    countArticlesInYear(year) {
      let count = 0;
      Object.values(year).forEach(month => {
        count += month.length;
      });
      return count;
    }
  }
}
</script>

<style scoped>
.archive-container {
  max-width: 900px;
  margin: 0 auto;
  padding: 2rem 1rem;
}

.archive-header {
  text-align: center;
  margin-bottom: 2rem;
}

.archive-header h1 {
  font-size: 2.5rem;
  color: #333;
  margin-bottom: 0.5rem;
}

.archive-header p {
  font-size: 1.2rem;
  color: #666;
}

.content-card {
  background: white;
  border-radius: 10px;
  padding: 2rem;
  box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
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

.archive-timeline {
  display: flex;
  flex-direction: column;
  gap: 2.5rem;
}

.archive-year {
  position: relative;
}

.year-header {
  display: flex;
  align-items: baseline;
  margin-bottom: 1.5rem;
  border-bottom: 2px solid #f0f0f0;
  padding-bottom: 0.5rem;
}

.year-header h2 {
  font-size: 2rem;
  color: #333;
  margin: 0;
  font-weight: 600;
}

.article-count {
  margin-left: 1rem;
  font-size: 1rem;
  color: #666;
}

.months-container {
  padding-left: 1.5rem;
  position: relative;
}

.months-container:before {
  content: '';
  position: absolute;
  left: 0;
  top: 0;
  bottom: 0;
  width: 2px;
  background-color: #f0f0f0;
}

.archive-month {
  margin-bottom: 2rem;
  position: relative;
}

.archive-month:last-child {
  margin-bottom: 0;
}

.month-header {
  font-size: 1.5rem;
  color: #444;
  margin: 0 0 1rem 0;
  position: relative;
}

.month-header:before {
  content: '';
  position: absolute;
  left: -1.5rem;
  top: 50%;
  transform: translateY(-50%);
  width: 12px;
  height: 12px;
  background-color: #6366f1;
  border-radius: 50%;
}

.article-list {
  list-style: none;
  padding: 0;
  margin: 0;
}

.article-item {
  display: flex;
  margin-bottom: 0.8rem;
  padding-left: 1rem;
  position: relative;
}

.article-item:before {
  content: '';
  position: absolute;
  left: 0;
  top: 0.6rem;
  width: 6px;
  height: 6px;
  background-color: #6366f1;
  border-radius: 50%;
}

.article-date {
  min-width: 50px;
  font-size: 0.9rem;
  color: #666;
  margin-right: 1rem;
}

.article-title {
  flex: 1;
}

.article-title a {
  color: #333;
  text-decoration: none;
  transition: color 0.2s;
}

.article-title a:hover {
  color: #6366f1;
}

@media (max-width: 768px) {
  .archive-header h1 {
    font-size: 2rem;
  }
  
  .content-card {
    padding: 1.5rem;
  }
  
  .year-header h2 {
    font-size: 1.8rem;
  }
}
</style> 