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
    
    <div v-else-if="article" class="article-page">
      <div class="article-content">
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
      
      <!-- 评论区 -->
      <div class="article-comments">
        <h3 class="comments-title">评论 ({{ comments.length }})</h3>
        
        <!-- 评论表单 -->
        <div class="comment-form">
          <h4>发表评论</h4>
          <div v-if="isLoggedIn">
            <textarea 
              v-model="newComment.content" 
              class="comment-textarea" 
              placeholder="请输入您的评论..."
              :disabled="submittingComment"
            ></textarea>
            <button 
              @click="postComment" 
              class="submit-comment"
              :disabled="!newComment.content || submittingComment"
            >
              {{ submittingComment ? '提交中...' : '发表评论' }}
            </button>
          </div>
          <div v-else class="anonymous-comment-form">
            <div class="form-row">
              <input 
                type="text" 
                v-model="newComment.author_name" 
                placeholder="您的昵称 *" 
                :disabled="submittingComment"
              />
              <input 
                type="email" 
                v-model="newComment.author_email" 
                placeholder="您的邮箱 *" 
                :disabled="submittingComment"
              />
            </div>
            <textarea 
              v-model="newComment.content" 
              class="comment-textarea" 
              placeholder="请输入您的评论..."
              :disabled="submittingComment"
            ></textarea>
            <button 
              @click="postAnonymousComment" 
              class="submit-comment"
              :disabled="!canSubmitAnonymousComment || submittingComment"
            >
              {{ submittingComment ? '提交中...' : '发表评论' }}
            </button>
            <div class="login-suggestion">
              已有账号？<router-link to="/login" class="login-link">登录后评论</router-link>
            </div>
          </div>
        </div>
        
        <!-- 评论列表 -->
        <div v-if="commentsLoading" class="comments-loading">
          <div class="loading-spinner"></div>
          <p>加载评论中...</p>
        </div>
        <div v-else-if="commentsError" class="comments-error">
          <p>{{ commentsErrorMessage }}</p>
          <button @click="fetchComments" class="btn-retry">重试</button>
        </div>
        <div v-else-if="comments.length === 0" class="no-comments">
          <p>暂无评论，快来发表第一条评论吧！</p>
        </div>
        <div v-else class="comments-list">
          <comment-item 
            v-for="comment in comments" 
            :key="comment.id" 
            :comment="comment"
            :article-id="articleId"
            @reply="replyToComment"
            @delete="deleteComment"
          />
        </div>
      </div>
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
import CommentItem from '../components/CommentItem.vue';

export default {
  name: 'ArticleDetail',
  components: {
    CommentItem
  },
  
  data() {
    return {
      article: null,
      articleId: 0,
      loading: true,
      error: false,
      errorMessage: '',
      comments: [],
      commentsLoading: false,
      commentsError: false,
      commentsErrorMessage: '',
      newComment: {
        content: '',
        author_name: '',
        author_email: ''
      },
      submittingComment: false,
      replyingToComment: null
    };
  },
  
  computed: {
    renderedContent() {
      if (!this.article || !this.article.content) return '';
      
      // 使用 marked 将 Markdown 转换为 HTML
      const rawHtml = marked(this.article.content);
      
      // 使用 DOMPurify 清理 HTML 防止 XSS 攻击
      return DOMPurify.sanitize(rawHtml);
    },
    
    isLoggedIn() {
      return localStorage.getItem('isLoggedIn') === 'true';
    },
    
    canSubmitAnonymousComment() {
      return this.newComment.content && 
             this.newComment.author_name && 
             this.newComment.author_email &&
             this.validateEmail(this.newComment.author_email);
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
      
      // 使用slug获取文章数据
      axios.get(`/api/articles/${slug}`)
        .then(response => {
          if (response.data.code === 0 && response.data.data && response.data.data.article) {
            this.article = response.data.data.article;
            this.articleId = this.article.id;
            
            // 更新URL，确保包含作者名
            if (this.article.author && this.$route.params.author !== this.article.author_username) {
              const authorUsername = this.article.author_username || 'author';
              this.$router.replace({
                name: 'article-detail',
                params: { 
                  author: authorUsername,
                  slug: slug 
                }
              });
            }
            
            // 文章加载成功后加载评论
            this.fetchComments();
            
            // 更新浏览器标题
            document.title = `${this.article.title} - Tina博客`;
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
    },
    
    fetchComments() {
      if (!this.articleId) return;
      
      this.commentsLoading = true;
      this.commentsError = false;
      
      axios.get(`/api/articles/${this.articleId}/comments`)
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.comments = response.data.data.comments || [];
          } else {
            this.commentsError = true;
            this.commentsErrorMessage = response.data.message || '获取评论失败';
          }
        })
        .catch(error => {
          this.commentsError = true;
          this.commentsErrorMessage = error.response?.data?.message || '获取评论时出错';
          console.error('获取评论失败:', error);
        })
        .finally(() => {
          this.commentsLoading = false;
        });
    },
    
    postComment() {
      if (!this.newComment.content || this.submittingComment) return;
      
      this.submittingComment = true;
      
      const commentData = {
        article_id: this.articleId,
        content: this.newComment.content,
        parent_id: this.replyingToComment ? this.replyingToComment.id : null
      };
      
      axios.post('/api/comments', commentData, {
        headers: this.getAuthHeaders()
      })
        .then(response => {
          if (response.data.code === 0) {
            // 评论成功，重新获取评论列表
            this.fetchComments();
            // 清空评论框
            this.newComment.content = '';
            this.replyingToComment = null;
          } else {
            alert(response.data.message || '发表评论失败');
          }
        })
        .catch(error => {
          const errorMsg = error.response?.data?.message || '发表评论时出错';
          alert(errorMsg);
          console.error('发表评论失败:', error);
        })
        .finally(() => {
          this.submittingComment = false;
        });
    },
    
    postAnonymousComment() {
      if (!this.canSubmitAnonymousComment || this.submittingComment) return;
      
      this.submittingComment = true;
      
      const commentData = {
        article_id: this.articleId,
        content: this.newComment.content,
        author_name: this.newComment.author_name,
        author_email: this.newComment.author_email,
        parent_id: this.replyingToComment ? this.replyingToComment.id : null
      };
      
      axios.post('/api/comments/anonymous', commentData)
        .then(response => {
          if (response.data.code === 0) {
            // 评论成功，重新获取评论列表
            this.fetchComments();
            // 清空评论框
            this.newComment.content = '';
            this.replyingToComment = null;
          } else {
            alert(response.data.message || '发表评论失败');
          }
        })
        .catch(error => {
          const errorMsg = error.response?.data?.message || '发表评论时出错';
          alert(errorMsg);
          console.error('发表评论失败:', error);
        })
        .finally(() => {
          this.submittingComment = false;
        });
    },
    
    replyToComment(comment) {
      this.replyingToComment = comment;
      this.newComment.content = `@${comment.author} `;
      
      // 滚动到评论框
      this.$nextTick(() => {
        const commentForm = document.querySelector('.comment-form');
        if (commentForm) {
          commentForm.scrollIntoView({ behavior: 'smooth' });
          const textarea = commentForm.querySelector('textarea');
          if (textarea) {
            textarea.focus();
          }
        }
      });
    },
    
    deleteComment(commentId) {
      if (!this.isLoggedIn || !commentId) return;
      
      if (!confirm('确定要删除这条评论吗？')) return;
      
      axios.delete(`/api/comments/${commentId}`, {
        headers: this.getAuthHeaders()
      })
        .then(response => {
          if (response.data.code === 0) {
            // 删除成功，重新获取评论列表
            this.fetchComments();
          } else {
            alert(response.data.message || '删除评论失败');
          }
        })
        .catch(error => {
          const errorMsg = error.response?.data?.message || '删除评论时出错';
          alert(errorMsg);
          console.error('删除评论失败:', error);
        });
    },
    
    getAuthHeaders() {
      const token = localStorage.getItem('token');
      return token ? { 'Authorization': `Bearer ${token}` } : {};
    },
    
    validateEmail(email) {
      const re = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
      return re.test(email);
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
  max-width: 1200px;
  margin: 0 auto;
  padding: 20px;
  display: flex;
  flex-direction: column;
}

.article-page {
  display: grid;
  grid-template-columns: 1fr;
  gap: 40px;
}

@media (min-width: 992px) {
  .article-page {
    grid-template-columns: 3fr 1fr;
  }
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

/* 评论区样式 */
.article-comments {
  margin-top: 40px;
  padding-top: 20px;
  border-top: 1px solid #eee;
}

.comments-title {
  font-size: 1.5rem;
  margin-bottom: 20px;
  color: #333;
}

.comment-form {
  background-color: #f9f9f9;
  padding: 20px;
  border-radius: 5px;
  margin-bottom: 30px;
}

.comment-form h4 {
  margin-top: 0;
  margin-bottom: 15px;
  color: #333;
}

.comment-textarea {
  width: 100%;
  min-height: 100px;
  padding: 10px;
  margin-bottom: 10px;
  border: 1px solid #ddd;
  border-radius: 4px;
  resize: vertical;
}

.anonymous-comment-form .form-row {
  display: flex;
  gap: 10px;
  margin-bottom: 10px;
}

.anonymous-comment-form input {
  flex: 1;
  padding: 8px 10px;
  border: 1px solid #ddd;
  border-radius: 4px;
}

.submit-comment {
  padding: 8px 16px;
  background-color: #3498db;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

.submit-comment:hover {
  background-color: #2980b9;
}

.submit-comment:disabled {
  background-color: #95a5a6;
  cursor: not-allowed;
}

.login-suggestion {
  margin-top: 10px;
  font-size: 0.9rem;
  color: #666;
}

.login-link {
  color: #3498db;
  text-decoration: none;
}

.login-link:hover {
  text-decoration: underline;
}

.comments-loading, .comments-error, .no-comments {
  text-align: center;
  padding: 20px 0;
  color: #666;
}

.comments-list {
  margin-top: 20px;
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
  font-size: 1.6rem;
  margin: 1.4rem 0 0.8rem;
  padding-bottom: 0.2rem;
}

.article-body h3 {
  font-size: 1.4rem;
  margin: 1.3rem 0 0.7rem;
}

.article-body h4 {
  font-size: 1.2rem;
  margin: 1.2rem 0 0.6rem;
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

.article-body blockquote {
  border-left: 4px solid #3498db;
  padding-left: 1rem;
  margin-left: 0;
  color: #555;
  font-style: italic;
}

.article-body code {
  background-color: #f8f8f8;
  padding: 0.2rem 0.4rem;
  border-radius: 3px;
  font-family: monospace;
  font-size: 0.9em;
}

.article-body pre {
  background-color: #f8f8f8;
  padding: 1rem;
  border-radius: 5px;
  overflow-x: auto;
}

.article-body pre code {
  padding: 0;
  background-color: transparent;
}

.article-body table {
  border-collapse: collapse;
  width: 100%;
  margin: 1rem 0;
}

.article-body table th,
.article-body table td {
  border: 1px solid #ddd;
  padding: 0.5rem;
}

.article-body table th {
  background-color: #f8f8f8;
  font-weight: bold;
}

.article-body ul, .article-body ol {
  padding-left: 2rem;
  margin: 0.8rem 0;
}

.article-body hr {
  border: none;
  border-top: 1px solid #eee;
  margin: 1.5rem 0;
}
</style> 