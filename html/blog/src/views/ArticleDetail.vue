<template>
  <div class="article-detail-container">
    <div v-if="loading" class="loading-container">
      <div class="loading-spinner"></div>
      <p>正在加载文章...</p>
    </div>
    
    <div v-else-if="error" class="error-container">
      <h2>加载文章时出错</h2>
      <p>{{ errorMessage }}</p>
      <button @click="fetchArticle" class="btn-retry">重试</button>
    </div>
    
    <div v-else-if="article" class="article-content">
      <h1 class="article-title">{{ article.title }}</h1>
      
      <div class="article-meta">
        <span class="article-author">作者: {{ article.author }}</span>
        <span class="article-date">发布于: {{ formatDate(article.created_at) }}</span>
        
        <div class="article-categories" v-if="article.categories && article.categories.length > 0">
          分类: 
          <router-link 
            v-for="(category, index) in article.categories" 
            :key="'cat-' + category.id"
            :to="{ path: '/', query: { category: category.slug } }"
            class="category-tag"
          >
            {{ category.name }}{{ index < article.categories.length - 1 ? ', ' : '' }}
          </router-link>
        </div>
        
        <div class="article-tags" v-if="article.tags && article.tags.length > 0">
          标签: 
          <router-link 
            v-for="(tag, index) in article.tags" 
            :key="'tag-' + tag.id"
            :to="{ path: '/', query: { tag: tag.slug } }"
            class="tag"
          >
            {{ tag.name }}{{ index < article.tags.length - 1 ? ', ' : '' }}
          </router-link>
        </div>
      </div>
      
      <div class="article-summary" v-if="article.summary">
        <strong>摘要:</strong> {{ article.summary }}
      </div>
      
      <div class="article-body" v-html="renderedContent"></div>
    </div>
    
    <div v-else class="no-article">
      <h2>未找到文章</h2>
      <p>该文章不存在或已被删除</p>
      <router-link to="/" class="btn-home">返回首页</router-link>
    </div>
  </div>
</template>

<script>
import axios from 'axios';
import { marked } from 'marked';
import DOMPurify from 'dompurify';

export default {
  name: 'ArticleDetail',
  
  data() {
    return {
      article: null,
      loading: true,
      error: false,
      errorMessage: ''
    };
  },
  
  computed: {
    renderedContent() {
      if (!this.article || !this.article.content) return '';
      
      // 使用 marked 将 Markdown 转换为 HTML
      const rawHtml = marked(this.article.content);
      
      // 使用 DOMPurify 清理 HTML 防止 XSS 攻击
      return DOMPurify.sanitize(rawHtml);
    }
  },
  
  methods: {
    formatDate(dateString) {
      if (!dateString) return '';
      
      const options = { 
        year: 'numeric', 
        month: 'long', 
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit'
      };
      
      return new Date(dateString).toLocaleDateString('zh-CN', options);
    },
    
    fetchArticle() {
      const slug = this.$route.params.slug;
      if (!slug) {
        this.error = true;
        this.errorMessage = '文章标识不能为空';
        this.loading = false;
        return;
      }
      
      this.loading = true;
      this.error = false;
      
      axios.get(`/api/articles/${slug}`)
        .then(response => {
          if (response.data.code === 0 && response.data.data && response.data.data.article) {
            this.article = response.data.data.article;
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取文章失败';
          }
        })
        .catch(error => {
          this.error = true;
          this.errorMessage = error.response?.data?.message || '获取文章时出错';
          console.error('获取文章详情失败:', error);
        })
        .finally(() => {
          this.loading = false;
        });
    }
  },
  
  created() {
    this.fetchArticle();
  },
  
  watch: {
    '$route.params.slug'(newSlug) {
      if (newSlug) {
        this.fetchArticle();
      }
    }
  }
};
</script>

<style scoped>
.article-detail-container {
  max-width: 900px;
  margin: 0 auto;
  padding: 20px;
}

.loading-container, .error-container, .no-article {
  text-align: center;
  padding: 40px 0;
}

.loading-spinner {
  border: 4px solid rgba(0, 0, 0, 0.1);
  border-radius: 50%;
  border-top: 4px solid #3498db;
  width: 40px;
  height: 40px;
  margin: 0 auto 20px;
  animation: spin 1s linear infinite;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

.article-title {
  font-size: 2.2rem;
  margin-bottom: 15px;
  color: #333;
}

.article-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 15px;
  margin-bottom: 25px;
  font-size: 0.9rem;
  color: #666;
  border-bottom: 1px solid #eee;
  padding-bottom: 15px;
}

.article-author, .article-date {
  display: inline-block;
}

.article-categories, .article-tags {
  margin-top: 10px;
  width: 100%;
}

.category-tag, .tag {
  color: #3498db;
  text-decoration: none;
}

.category-tag:hover, .tag:hover {
  text-decoration: underline;
}

.article-summary {
  background-color: #f9f9f9;
  padding: 15px;
  border-left: 3px solid #3498db;
  margin-bottom: 25px;
  font-style: italic;
}

.article-body {
  line-height: 1.7;
  font-size: 1.1rem;
}

.btn-retry, .btn-home {
  display: inline-block;
  margin-top: 15px;
  padding: 8px 16px;
  background-color: #3498db;
  color: white;
  text-decoration: none;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 1rem;
}

.btn-retry:hover, .btn-home:hover {
  background-color: #2980b9;
}
</style>

<style>
/* 非 scoped 样式，适用于文章内容渲染后的 HTML */
.article-body h1 {
  font-size: 1.8rem;
  margin: 1.5rem 0 1rem;
  padding-bottom: 0.3rem;
  border-bottom: 1px solid #eee;
}

.article-body h2 {
  font-size: 1.5rem;
  margin: 1.2rem 0 0.8rem;
}

.article-body h3 {
  font-size: 1.3rem;
  margin: 1rem 0 0.6rem;
}

.article-body p {
  margin: 0.8rem 0;
}

.article-body img {
  max-width: 100%;
  height: auto;
  display: block;
  margin: 1.5rem auto;
}

.article-body a {
  color: #3498db;
  text-decoration: none;
}

.article-body a:hover {
  text-decoration: underline;
}

.article-body pre {
  background-color: #f6f8fa;
  border-radius: 3px;
  padding: 16px;
  overflow: auto;
  font-family: monospace;
  margin: 1rem 0;
}

.article-body blockquote {
  margin: 1rem 0;
  padding: 0.5rem 1rem;
  border-left: 4px solid #ddd;
  color: #666;
}

.article-body ul, .article-body ol {
  margin: 1rem 0;
  padding-left: 2rem;
}

.article-body table {
  border-collapse: collapse;
  width: 100%;
  margin: 1rem 0;
}

.article-body table th, .article-body table td {
  border: 1px solid #ddd;
  padding: 8px;
}

.article-body table th {
  background-color: #f2f2f2;
  text-align: left;
}
</style> 