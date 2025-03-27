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
      <!-- 左侧文章内容 -->
      <div class="article-main">
        <div class="article-header">
          <h1 class="article-title">{{ article.title }}</h1>
          
          <div class="article-meta">
            <div class="meta-item author">
              <span class="meta-icon">👤</span>
              <span>{{ article.author }}</span>
            </div>
            <div class="meta-item date">
              <span class="meta-icon">📅</span>
              <span>{{ formatDate(article.created_at) }}</span>
            </div>
            <div class="meta-item views" v-if="article.view_count !== undefined">
              <span class="meta-icon">👁️</span>
              <span>{{ article.view_count }} 次阅读</span>
            </div>
          </div>
          
          <div class="article-summary" v-if="article.summary">
            <strong>摘要:</strong> {{ article.summary }}
          </div>
        </div>
        
        <div class="article-content">
          <div class="article-body" v-html="renderedContent"></div>
          
          <!-- 文章标签和分类 -->
          <div class="article-tags-categories">
            <div class="article-categories" v-if="article.categories && article.categories.length > 0">
              <span class="label">分类:</span>
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
              <span class="label">标签:</span>
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
          
          <!-- 评论区 -->
          <div class="article-comments">
            <h3 class="comments-title">评论区 <span class="comments-count">({{ comments.length }})</span></h3>
            
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
      </div>
      
      <!-- 右侧边栏 -->
      <div class="article-sidebar">
        <div class="sidebar-card author-card">
          <h3>关于作者</h3>
          <div class="author-info">
            <div class="author-avatar">{{ article.author?.charAt(0) || 'A' }}</div>
            <div class="author-name">{{ article.author }}</div>
          </div>
          <div class="author-bio" v-if="article.author_bio">{{ article.author_bio }}</div>
        </div>
        
        <div class="sidebar-card toc-card" v-if="toc.length > 0">
          <h3>目录</h3>
          <ul class="toc-list">
            <li v-for="(item, index) in toc" :key="index" :class="{ 'toc-h2': item.level === 2, 'toc-h3': item.level === 3 }">
              <a :href="'#' + item.id">{{ item.text }}</a>
            </li>
          </ul>
        </div>
        
        <div class="sidebar-card related-card" v-if="relatedArticles.length > 0">
          <h3>相关文章</h3>
          <ul class="related-list">
            <li v-for="(article, index) in relatedArticles" :key="index">
              <router-link :to="`/article/${article.author_username || 'author'}/${article.slug}`">
                {{ article.title }}
              </router-link>
            </li>
          </ul>
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
      replyingToComment: null,
      toc: [],
      relatedArticles: []
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
            
            // 获取作者用户名 - 如果API返回了author_username则使用，否则使用author
            const authorName = this.article.author_username || this.article.author || 'anonymous';
            
            // 更新URL，确保包含正确的作者名
            if (this.$route.params.author !== authorName && authorName !== 'anonymous') {
              console.log(`更新URL作者: ${this.$route.params.author} -> ${authorName}`);
              this.$router.replace({
                name: 'article-detail',
                params: { 
                  author: authorName,
                  slug: slug 
                }
              });
            }
            
            // 文章加载成功后加载评论
            this.fetchComments();
            
            // 生成目录
            this.generateToc();
            
            // 获取相关文章
            this.fetchRelatedArticles();
            
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
      
      console.log('开始获取文章ID为', this.articleId, '的评论');
      this.commentsLoading = true;
      this.commentsError = false;
      
      axios.get(`/api/articles/${this.articleId}/comments`)
        .then(response => {
          console.log('获取评论成功:', response.data);
          if (response.data.code === 0 && response.data.data) {
            this.comments = response.data.data.comments || [];
            console.log('更新评论列表，数量:', this.comments.length);
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
            console.log('评论发表成功，ID:', response.data.data.id);
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
            console.log('匿名评论发表成功，ID:', response.data.data.id);
            // 评论成功，重新获取评论列表
            this.fetchComments();
            // 清空评论框
            this.newComment.content = '';
            this.newComment.author_name = '';
            this.newComment.author_email = '';
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
    },
    
    generateToc() {
      this.$nextTick(() => {
        const headings = document.querySelectorAll('.article-body h1, .article-body h2, .article-body h3');
        this.toc = [];
        
        headings.forEach((heading, index) => {
          const id = `heading-${index}`;
          heading.id = id;
          
          this.toc.push({
            id,
            level: parseInt(heading.tagName.substr(1)),
            text: heading.textContent
          });
        });
      });
    },
    
    fetchRelatedArticles() {
      // 如果后端有API支持，可以调用API获取相关文章
      
      // 替代方案：获取同一作者的文章或同一分类的文章
      if (this.article) {
        const authorName = this.article.author || '';
        const categoryId = this.article.categories && this.article.categories.length > 0 
          ? this.article.categories[0].id 
          : null;
          
        // 这里仅模拟数据，实际应当调用API
        // 相关文章可以是：
        // 1. 同一作者的其他文章
        // 2. 同一分类的其他文章
        // 3. 同一标签的其他文章
        
        // 在实际项目中，添加相应的API:
        // /api/articles/related?author=${authorName}&exclude=${this.articleId}
        // 或
        // /api/articles/related?category=${categoryId}&exclude=${this.articleId}
        
        this.relatedArticles = [
          { 
            id: 101, 
            title: `${authorName}的其他文章1`, 
            slug: 'related-article-1', 
            author_username: authorName 
          },
          { 
            id: 102, 
            title: `${authorName}的其他文章2`, 
            slug: 'related-article-2', 
            author_username: authorName 
          },
          { 
            id: 103, 
            title: `相关分类文章`, 
            slug: 'related-category-article', 
            author_username: 'other-author' 
          }
        ];
      }
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
}

.loading-container, .error-container, .no-article {
  text-align: center;
  padding: 40px 0;
  margin: 20px;
  background-color: white;
  border-radius: 8px;
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
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

/* 文章页面布局 */
.article-page {
  display: grid;
  grid-template-columns: 1fr;
  gap: 20px;
}

@media (min-width: 992px) {
  .article-page {
    grid-template-columns: 3fr 1fr;
  }
}

/* 主内容区 */
.article-main {
  background-color: white;
  border-radius: 8px;
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
  overflow: hidden;
}

.article-header {
  padding: 30px 30px 0;
}

.article-title {
  font-size: 2.4rem;
  margin-bottom: 20px;
  color: #333;
  line-height: 1.3;
}

.article-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 20px;
  margin-bottom: 20px;
  font-size: 0.95rem;
  color: #666;
}

.meta-item {
  display: flex;
  align-items: center;
  gap: 5px;
}

.meta-icon {
  font-size: 1.1rem;
}

.article-summary {
  background-color: #f9f9f9;
  padding: 15px;
  border-left: 3px solid #3498db;
  margin-bottom: 25px;
  font-style: italic;
  border-radius: 0 4px 4px 0;
}

.article-content {
  padding: 0 30px 30px;
}

.article-body {
  line-height: 1.8;
  font-size: 1.1rem;
  color: #333;
  margin-bottom: 30px;
}

.article-tags-categories {
  border-top: 1px solid #eee;
  padding-top: 20px;
  display: flex;
  flex-direction: column;
  gap: 10px;
  font-size: 0.95rem;
}

.article-tags, .article-categories {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 10px;
}

.label {
  color: #666;
  font-weight: 500;
}

.category-tag, .tag {
  display: inline-block;
  padding: 4px 10px;
  background-color: #e9f5ff;
  color: #3498db;
  border-radius: 20px;
  text-decoration: none;
  transition: all 0.2s;
}

.tag {
  background-color: #f0f0f0;
  color: #666;
}

.category-tag:hover, .tag:hover {
  background-color: #3498db;
  color: white;
}

/* 评论区样式 */
.article-comments {
  margin-top: 40px;
  padding-top: 20px;
  border-top: 1px solid #eee;
}

.comments-title {
  font-size: 1.6rem;
  margin-bottom: 25px;
  color: #333;
  display: flex;
  align-items: center;
  gap: 10px;
}

.comments-count {
  font-size: 1.1rem;
  color: #666;
  font-weight: normal;
}

.comment-form {
  background-color: #f9f9f9;
  padding: 20px;
  border-radius: 8px;
  margin-bottom: 30px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
}

.comment-form h4 {
  margin-top: 0;
  margin-bottom: 15px;
  color: #333;
  font-size: 1.2rem;
}

.comment-textarea {
  width: 100%;
  min-height: 120px;
  padding: 12px;
  margin-bottom: 15px;
  border: 1px solid #ddd;
  border-radius: 4px;
  resize: vertical;
  font-family: inherit;
  font-size: 1rem;
  transition: border-color 0.2s;
}

.comment-textarea:focus {
  border-color: #3498db;
  outline: none;
}

.anonymous-comment-form .form-row {
  display: flex;
  gap: 15px;
  margin-bottom: 15px;
}

.anonymous-comment-form input {
  flex: 1;
  padding: 10px 12px;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-family: inherit;
  font-size: 1rem;
  transition: border-color 0.2s;
}

.anonymous-comment-form input:focus {
  border-color: #3498db;
  outline: none;
}

.submit-comment {
  padding: 10px 20px;
  background-color: #3498db;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-weight: 500;
  font-size: 1rem;
  transition: background-color 0.2s;
}

.submit-comment:hover {
  background-color: #2980b9;
}

.submit-comment:disabled {
  background-color: #95a5a6;
  cursor: not-allowed;
}

.login-suggestion {
  margin-top: 15px;
  font-size: 0.9rem;
  color: #666;
}

.login-link {
  color: #3498db;
  text-decoration: none;
  font-weight: 500;
}

.login-link:hover {
  text-decoration: underline;
}

.comments-loading, .comments-error, .no-comments {
  text-align: center;
  padding: 30px 0;
  color: #666;
  background-color: #f9f9f9;
  border-radius: 8px;
  margin-bottom: 20px;
}

.comments-list {
  margin-top: 30px;
}

.btn-retry, .btn-home {
  display: inline-block;
  margin-top: 15px;
  padding: 10px 20px;
  background-color: #3498db;
  color: white;
  text-decoration: none;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 1rem;
  font-weight: 500;
  transition: background-color 0.2s;
}

.btn-retry:hover, .btn-home:hover {
  background-color: #2980b9;
}

/* 侧边栏样式 */
.article-sidebar {
  align-self: start;
  position: sticky;
  top: 80px;
}

.sidebar-card {
  background-color: white;
  border-radius: 8px;
  padding: 20px;
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
  margin-bottom: 20px;
}

.sidebar-card h3 {
  margin-top: 0;
  margin-bottom: 15px;
  font-size: 1.2rem;
  color: #333;
  border-bottom: 2px solid #f0f0f0;
  padding-bottom: 10px;
}

.author-info {
  display: flex;
  align-items: center;
  gap: 15px;
  margin-bottom: 15px;
}

.author-avatar {
  width: 50px;
  height: 50px;
  border-radius: 50%;
  background-color: #3498db;
  color: white;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 1.5rem;
  font-weight: bold;
}

.author-name {
  font-size: 1.1rem;
  font-weight: 500;
  color: #333;
}

.author-bio {
  font-size: 0.9rem;
  color: #666;
  line-height: 1.6;
}

.toc-list, .related-list {
  list-style: none;
  padding: 0;
  margin: 0;
}

.toc-list li {
  margin-bottom: 10px;
}

.toc-list li a {
  color: #666;
  text-decoration: none;
  font-size: 0.95rem;
  display: block;
  padding: 5px 0;
  transition: color 0.2s;
}

.toc-list li a:hover {
  color: #3498db;
}

.toc-h2 {
  padding-left: 0;
}

.toc-h3 {
  padding-left: 20px;
}

.related-list li {
  margin-bottom: 12px;
  border-bottom: 1px solid #f0f0f0;
  padding-bottom: 12px;
}

.related-list li:last-child {
  margin-bottom: 0;
  border-bottom: none;
  padding-bottom: 0;
}

.related-list li a {
  color: #333;
  text-decoration: none;
  font-size: 0.95rem;
  line-height: 1.4;
  display: block;
  transition: color 0.2s;
}

.related-list li a:hover {
  color: #3498db;
}

@media (max-width: 768px) {
  .article-title {
    font-size: 1.8rem;
  }
  
  .article-header, .article-content {
    padding-left: 20px;
    padding-right: 20px;
  }
  
  .anonymous-comment-form .form-row {
    flex-direction: column;
    gap: 10px;
  }
}
</style>

<style>
/* 非 scoped 样式，适用于文章内容渲染后的 HTML */
.article-body h1 {
  font-size: 1.8rem;
  margin: 2rem 0 1rem;
  padding-bottom: 0.3rem;
  border-bottom: 1px solid #eee;
}

.article-body h2 {
  font-size: 1.6rem;
  margin: 1.8rem 0 1rem;
  padding-bottom: 0.2rem;
}

.article-body h3 {
  font-size: 1.4rem;
  margin: 1.6rem 0 0.8rem;
}

.article-body h4 {
  font-size: 1.2rem;
  margin: 1.4rem 0 0.7rem;
}

.article-body p {
  margin: 1rem 0;
  line-height: 1.8;
}

.article-body img {
  max-width: 100%;
  height: auto;
  display: block;
  margin: 1.5rem auto;
  border-radius: 4px;
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
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
  padding: 10px 20px;
  margin: 1.5rem 0;
  background-color: #f8f9fa;
  color: #555;
  font-style: italic;
  border-radius: 0 4px 4px 0;
}

.article-body code {
  background-color: #f8f8f8;
  padding: 0.2rem 0.4rem;
  border-radius: 3px;
  font-family: Consolas, Monaco, 'Andale Mono', monospace;
  font-size: 0.9em;
  color: #e74c3c;
}

.article-body pre {
  background-color: #f8f8f8;
  padding: 1rem;
  border-radius: 5px;
  overflow-x: auto;
  margin: 1.5rem 0;
}

.article-body pre code {
  padding: 0;
  background-color: transparent;
  color: #333;
}

.article-body table {
  border-collapse: collapse;
  width: 100%;
  margin: 1.5rem 0;
  border: 1px solid #ddd;
}

.article-body table th,
.article-body table td {
  border: 1px solid #ddd;
  padding: 0.75rem;
}

.article-body table th {
  background-color: #f8f8f8;
  font-weight: bold;
  text-align: left;
}

.article-body table tr:nth-child(even) {
  background-color: #f9f9f9;
}

.article-body ul, .article-body ol {
  padding-left: 2rem;
  margin: 1rem 0;
}

.article-body li {
  margin-bottom: 0.5rem;
}

.article-body hr {
  border: none;
  border-top: 1px solid #eee;
  margin: 2rem 0;
}
</style> 