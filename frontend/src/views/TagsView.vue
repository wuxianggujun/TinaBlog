<template>
  <div class="tags-container">
    <div class="tags-header">
      <h1>标签云</h1>
      <p>探索不同话题的文章集合</p>
    </div>

    <div v-if="isLoading" class="loading-container">
      <div class="spinner"></div>
      <p>正在加载标签...</p>
    </div>

    <div v-else-if="error" class="error-container">
      <i class="fas fa-exclamation-circle"></i>
      <p>{{ errorMessage || '获取标签失败，请稍后再试' }}</p>
      <button @click="fetchTags" class="retry-btn">重试</button>
    </div>

    <div v-else-if="tags.length === 0" class="empty-container">
      <i class="fas fa-tags"></i>
      <p>暂无标签</p>
    </div>

    <div v-else class="tags-content">
      <div class="tag-groups">
        <div class="tag-group">
          <div class="tags-cloud">
            <router-link 
              v-for="tag in tags" 
              :key="tag.id" 
              :to="`/tag/${tag.slug}`"
              class="tag-item"
              :style="{ fontSize: calculateTagSize(tag.count) }"
            >
              #{{ tag.name }}
              <span class="tag-count">{{ tag.count }}</span>
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
  name: 'TagsView',
  data() {
    return {
      tags: [],
      isLoading: true,
      error: false,
      errorMessage: ''
    }
  },
  created() {
    this.fetchTags();
    // 设置页面标题
    document.title = '标签 - Tina博客';
  },
  methods: {
    fetchTags() {
      this.isLoading = true;
      this.error = false;
      
      axios.get('/api/tags')
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.tags = response.data.data.tags || [];
            // 按文章数量排序
            this.tags.sort((a, b) => (b.count || 0) - (a.count || 0));
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取标签失败';
          }
        })
        .catch(error => {
          console.error('获取标签失败:', error);
          this.error = true;
          this.errorMessage = error.response?.data?.message || '网络错误，请稍后再试';
        })
        .finally(() => {
          this.isLoading = false;
        });
    },
    calculateTagSize(count) {
      // 基础字体大小14px，最大30px
      const minSize = 14;
      const maxSize = 30;
      const minCount = 1;
      
      // 获取所有标签中最高的count值
      const maxCount = Math.max(...this.tags.map(tag => tag.count || 0), minCount);
      
      // 如果所有标签都是相同的count，返回默认尺寸
      if (maxCount === minCount) return `${minSize}px`;
      
      // 计算大小比例
      let size = minSize + (count - minCount) * (maxSize - minSize) / (maxCount - minCount);
      // 确保在范围内
      size = Math.max(minSize, Math.min(maxSize, size));
      return `${size}px`;
    }
  }
}
</script>

<style scoped>
.tags-container {
  max-width: 1200px;
  margin: 0 auto;
  padding: 2rem 1rem;
}

.tags-header {
  text-align: center;
  margin-bottom: 3rem;
}

.tags-header h1 {
  font-size: 2.5rem;
  margin-bottom: 0.5rem;
  color: #333;
}

.tags-header p {
  color: #666;
  font-size: 1.1rem;
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

.tags-content {
  background-color: white;
  border-radius: 8px;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
  padding: 2rem;
}

.tag-groups {
  margin: 0 auto;
}

.tags-cloud {
  display: flex;
  flex-wrap: wrap;
  gap: 1rem;
  justify-content: center;
  padding: 1rem;
}

.tag-item {
  display: inline-flex;
  align-items: center;
  padding: 0.5rem 1.2rem;
  background-color: #f7f7f7;
  border-radius: 30px;
  color: #555;
  text-decoration: none;
  transition: all 0.3s;
  font-weight: 500;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
}

.tag-item:hover {
  background-color: #eef2ff;
  color: #4f46e5;
  transform: translateY(-3px) scale(1.05);
  box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
}

.tag-count {
  margin-left: 0.5rem;
  background-color: #6366f1;
  color: white;
  padding: 0.1rem 0.5rem;
  border-radius: 10px;
  font-size: 0.75rem;
}

@media (max-width: 768px) {
  .tags-header h1 {
    font-size: 2rem;
  }
  
  .tags-content {
    padding: 1.5rem 1rem;
  }
  
  .tag-item {
    padding: 0.4rem 1rem;
  }
}
</style> 