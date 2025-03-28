<template>
  <div class="comments-view">
    <div class="filter-bar">
      <div class="filter-group">
        <select v-model="filters.status" class="filter-select" @change="loadComments">
          <option value="all">所有评论</option>
          <option value="approved">已批准</option>
          <option value="pending">待审核</option>
        </select>
        
        <select v-model="filters.sortBy" class="filter-select" @change="loadComments">
          <option value="newest">最新评论</option>
          <option value="oldest">最早评论</option>
        </select>
      </div>
      
      <div class="search-box">
        <input 
          type="text" 
          v-model="searchQuery" 
          placeholder="搜索评论..." 
          class="search-input"
          @input="debounceSearch"
        />
        <button class="search-btn">
          <span class="search-icon">🔍</span>
        </button>
      </div>
    </div>
    
    <!-- 批量操作按钮 -->
    <div class="batch-actions" v-if="selectedComments.length > 0">
      <span class="batch-info">已选择 {{ selectedComments.length }} 条评论</span>
      <button class="batch-btn approve" @click="batchApprove">批量批准</button>
      <button class="batch-btn delete" @click="confirmBatchDelete">批量删除</button>
    </div>
    
    <!-- 评论列表 -->
    <div class="table-container">
      <table class="data-table" v-if="comments.length > 0">
        <thead>
          <tr>
            <th class="th-checkbox">
              <input type="checkbox" v-model="selectAll" @change="toggleSelectAll">
            </th>
            <th class="th-author">评论者</th>
            <th class="th-comment">评论内容</th>
            <th class="th-article">评论文章</th>
            <th class="th-date">评论时间</th>
            <th class="th-actions">操作</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="comment in comments" :key="comment.id" :class="['table-row', { selected: selectedComments.includes(comment.id) }]">
            <td class="td-checkbox">
              <input type="checkbox" :value="comment.id" v-model="selectedComments">
            </td>
            <td class="td-author">
              <div class="author-info">
                <div class="author-name">{{ comment.author_name || getUsername(comment.user_uuid) }}</div>
                <div class="author-email" v-if="comment.author_email">{{ comment.author_email }}</div>
              </div>
            </td>
            <td class="td-comment">
              <div class="comment-content">
                {{ truncateContent(comment.content) }}
              </div>
              <div v-if="comment.parent_id" class="reply-info">
                回复: {{ getParentCommentSummary(comment.parent_id) }}
              </div>
            </td>
            <td class="td-article">
              <router-link :to="`/article/${getArticleAuthor(comment.article_id)}/${getArticleSlug(comment.article_id)}`" class="article-link">
                {{ getArticleTitle(comment.article_id) }}
              </router-link>
            </td>
            <td class="td-date">{{ formatDate(comment.created_at) }}</td>
            <td class="td-actions">
              <div class="action-buttons">
                <button @click="replyToComment(comment)" class="action-btn reply" title="回复">
                  💬
                </button>
                <button @click="approveComment(comment)" class="action-btn approve" title="批准" v-if="!comment.is_approved">
                  ✅
                </button>
                <button @click="confirmDeleteComment(comment)" class="action-btn delete" title="删除">
                  🗑️
                </button>
              </div>
            </td>
          </tr>
        </tbody>
      </table>
      <div v-else class="empty-state">
        <div class="empty-icon">💬</div>
        <h3>暂无评论</h3>
        <p>{{ getEmptyStateMessage() }}</p>
      </div>
    </div>
    
    <!-- 分页控件 -->
    <div class="pagination" v-if="totalPages > 1">
      <button 
        class="page-btn" 
        :disabled="currentPage === 1"
        @click="changePage(currentPage - 1)"
      >
        上一页
      </button>
      <div class="page-numbers">
        <template v-for="page in paginationRange" :key="page">
          <button 
            v-if="page !== '...'" 
            :class="['page-number', { active: page === currentPage }]"
            @click="changePage(page)"
          >
            {{ page }}
          </button>
          <span v-else class="ellipsis">...</span>
        </template>
      </div>
      <button 
        class="page-btn" 
        :disabled="currentPage === totalPages"
        @click="changePage(currentPage + 1)"
      >
        下一页
      </button>
    </div>
    
    <!-- 回复评论弹窗 -->
    <div v-if="replyDialogVisible" class="dialog-backdrop">
      <div class="dialog">
        <div class="dialog-header">
          <h3>回复评论</h3>
          <button class="close-btn" @click="closeReplyDialog">×</button>
        </div>
        <div class="dialog-body">
          <div class="original-comment">
            <div class="comment-author">
              {{ currentComment ? (currentComment.author_name || getUsername(currentComment.user_uuid)) : '' }}:
            </div>
            <div class="comment-text">
              {{ currentComment ? currentComment.content : '' }}
            </div>
          </div>
          <textarea 
            v-model="replyContent" 
            class="reply-textarea" 
            placeholder="输入您的回复..."
            rows="5"
          ></textarea>
        </div>
        <div class="dialog-footer">
          <button class="cancel-btn" @click="closeReplyDialog">取消</button>
          <button class="submit-btn" @click="submitReply" :disabled="!replyContent.trim()">提交回复</button>
        </div>
      </div>
    </div>
    
    <!-- 删除确认弹窗 -->
    <div v-if="deleteDialogVisible" class="dialog-backdrop">
      <div class="dialog">
        <div class="dialog-header">
          <h3>确认删除</h3>
          <button class="close-btn" @click="closeDeleteDialog">×</button>
        </div>
        <div class="dialog-body">
          <p>确定要删除这条评论吗？此操作无法撤销。</p>
          <div class="comment-preview" v-if="currentComment">
            <div class="comment-text">{{ currentComment.content }}</div>
          </div>
        </div>
        <div class="dialog-footer">
          <button class="cancel-btn" @click="closeDeleteDialog">取消</button>
          <button class="delete-btn" @click="deleteComment">确认删除</button>
        </div>
      </div>
    </div>
    
    <!-- 批量删除确认弹窗 -->
    <div v-if="batchDeleteDialogVisible" class="dialog-backdrop">
      <div class="dialog">
        <div class="dialog-header">
          <h3>批量删除评论</h3>
          <button class="close-btn" @click="closeBatchDeleteDialog">×</button>
        </div>
        <div class="dialog-body">
          <p>确定要删除选中的 {{ selectedComments.length }} 条评论吗？此操作无法撤销。</p>
        </div>
        <div class="dialog-footer">
          <button class="cancel-btn" @click="closeBatchDeleteDialog">取消</button>
          <button class="delete-btn" @click="executeBatchDelete">确认删除</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';

export default {
  name: 'CommentsView',
  props: {
    userInfo: {
      type: Object,
      required: true
    },
    articles: {
      type: Array,
      required: true
    }
  },
  data() {
    return {
      comments: [],
      filters: {
        status: 'all',
        sortBy: 'newest'
      },
      searchQuery: '',
      currentPage: 1,
      totalComments: 0,
      itemsPerPage: 10,
      selectedComments: [],
      selectAll: false,
      debounceTimeout: null,
      
      // 回复相关
      replyDialogVisible: false,
      currentComment: null,
      replyContent: '',
      
      // 删除相关
      deleteDialogVisible: false,
      batchDeleteDialogVisible: false,
      
      // 用户信息缓存
      userCache: {},
      
      // 加载状态
      isLoading: false
    };
  },
  computed: {
    totalPages() {
      return Math.ceil(this.totalComments / this.itemsPerPage);
    },
    
    paginationRange() {
      const range = [];
      const maxVisible = 5;
      const halfVisible = Math.floor(maxVisible / 2);
      
      if (this.totalPages <= maxVisible) {
        // 如果总页数少于最大可见页数，显示所有页码
        for (let i = 1; i <= this.totalPages; i++) {
          range.push(i);
        }
      } else {
        // 总是显示第一页
        range.push(1);
        
        // 确定中间部分
        let startPage = Math.max(2, this.currentPage - halfVisible);
        let endPage = Math.min(this.totalPages - 1, this.currentPage + halfVisible);
        
        // 调整以确保我们总是显示相同数量的页码
        if (startPage > 2) range.push('...');
        
        for (let i = startPage; i <= endPage; i++) {
          range.push(i);
        }
        
        if (endPage < this.totalPages - 1) range.push('...');
        
        // 总是显示最后一页
        range.push(this.totalPages);
      }
      
      return range;
    }
  },
  mounted() {
    this.loadComments();
  },
  methods: {
    formatDate(dateString) {
      if (!dateString) return '';
      const date = new Date(dateString);
      return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')} ${String(date.getHours()).padStart(2, '0')}:${String(date.getMinutes()).padStart(2, '0')}`;
    },
    
    truncateContent(content, maxLength = 100) {
      if (!content) return '';
      return content.length > maxLength 
        ? content.substring(0, maxLength) + '...' 
        : content;
    },
    
    getUsername(userUuid) {
      if (!userUuid) return '匿名用户';
      
      // 使用缓存减少查询
      if (this.userCache[userUuid]) {
        return this.userCache[userUuid];
      }
      
      // 查找用户信息
      // 在实际应用中，应当使用API请求获取用户信息，此处为简化示例
      return '用户';
    },
    
    getArticleTitle(articleId) {
      const article = this.articles.find(a => a.id === articleId);
      return article ? article.title : '未知文章';
    },
    
    getArticleSlug(articleId) {
      const article = this.articles.find(a => a.id === articleId);
      return article ? article.slug : '';
    },
    
    getArticleAuthor(articleId) {
      const article = this.articles.find(a => a.id === articleId);
      if (!article) return '';
      
      return this.userInfo.username;
    },
    
    getParentCommentSummary(parentId) {
      const parentComment = this.comments.find(c => c.id === parentId);
      if (!parentComment) return '原评论';
      
      return this.truncateContent(parentComment.content, 30);
    },
    
    loadComments() {
      this.isLoading = true;
      
      // 重置选择状态
      this.selectedComments = [];
      this.selectAll = false;
      
      // 构建API查询参数
      const params = {
        page: this.currentPage,
        pageSize: this.itemsPerPage,
        status: this.filters.status,
        sortBy: this.filters.sortBy
      };
      
      if (this.searchQuery.trim()) {
        params.search = this.searchQuery.trim();
      }
      
      // 发送API请求获取评论
      axios.get('/api/admin/comments', { params })
        .then(response => {
          if (response.data.code === 0) {
            this.comments = response.data.data.comments || [];
            this.totalComments = response.data.data.pagination?.total || 0;
          } else {
            console.error('获取评论失败:', response.data.message);
            this.comments = [];
            this.totalComments = 0;
          }
        })
        .catch(error => {
          console.error('获取评论请求错误:', error);
          this.comments = [];
          this.totalComments = 0;
        })
        .finally(() => {
          this.isLoading = false;
        });
    },
    
    debounceSearch() {
      clearTimeout(this.debounceTimeout);
      this.debounceTimeout = setTimeout(() => {
        this.currentPage = 1;
        this.loadComments();
      }, 300);
    },
    
    changePage(page) {
      if (page >= 1 && page <= this.totalPages) {
        this.currentPage = page;
        this.loadComments();
        window.scrollTo(0, 0);
      }
    },
    
    toggleSelectAll() {
      if (this.selectAll) {
        this.selectedComments = this.comments.map(comment => comment.id);
      } else {
        this.selectedComments = [];
      }
    },
    
    replyToComment(comment) {
      this.currentComment = comment;
      this.replyContent = '';
      this.replyDialogVisible = true;
    },
    
    closeReplyDialog() {
      this.replyDialogVisible = false;
      this.currentComment = null;
      this.replyContent = '';
    },
    
    submitReply() {
      if (!this.currentComment || !this.replyContent.trim()) return;
      
      const data = {
        article_id: this.currentComment.article_id,
        content: this.replyContent.trim(),
        parent_id: this.currentComment.id
      };
      
      axios.post('/api/comments', data)
        .then(response => {
          if (response.data.code === 0) {
            this.closeReplyDialog();
            this.loadComments();
          } else {
            console.error('回复评论失败:', response.data.message);
            alert('回复评论失败: ' + response.data.message);
          }
        })
        .catch(error => {
          console.error('回复评论请求错误:', error);
          alert('回复评论失败，请稍后重试');
        });
    },
    
    approveComment(comment) {
      axios.post(`/api/admin/comments/${comment.id}/approve`)
        .then(response => {
          if (response.data.code === 0) {
            // 更新本地评论状态
            const index = this.comments.findIndex(c => c.id === comment.id);
            if (index !== -1) {
              this.comments[index].is_approved = true;
            }
          } else {
            console.error('批准评论失败:', response.data.message);
            alert('批准评论失败: ' + response.data.message);
          }
        })
        .catch(error => {
          console.error('批准评论请求错误:', error);
          alert('批准评论失败，请稍后重试');
        });
    },
    
    confirmDeleteComment(comment) {
      this.currentComment = comment;
      this.deleteDialogVisible = true;
    },
    
    closeDeleteDialog() {
      this.deleteDialogVisible = false;
      this.currentComment = null;
    },
    
    deleteComment() {
      if (!this.currentComment) return;
      
      axios.delete(`/api/comments/${this.currentComment.id}`)
        .then(response => {
          if (response.data.code === 0) {
            this.closeDeleteDialog();
            // 从列表中移除已删除的评论
            this.comments = this.comments.filter(c => c.id !== this.currentComment.id);
            // 从选中列表中移除
            this.selectedComments = this.selectedComments.filter(id => id !== this.currentComment.id);
          } else {
            console.error('删除评论失败:', response.data.message);
            alert('删除评论失败: ' + response.data.message);
          }
        })
        .catch(error => {
          console.error('删除评论请求错误:', error);
          alert('删除评论失败，请稍后重试');
        });
    },
    
    confirmBatchDelete() {
      if (this.selectedComments.length === 0) return;
      this.batchDeleteDialogVisible = true;
    },
    
    closeBatchDeleteDialog() {
      this.batchDeleteDialogVisible = false;
    },
    
    executeBatchDelete() {
      if (this.selectedComments.length === 0) {
        this.closeBatchDeleteDialog();
        return;
      }
      
      axios.post('/api/admin/comments/batch-delete', { comment_ids: this.selectedComments })
        .then(response => {
          if (response.data.code === 0) {
            this.closeBatchDeleteDialog();
            // 重新加载评论列表
            this.loadComments();
          } else {
            console.error('批量删除评论失败:', response.data.message);
            alert('批量删除评论失败: ' + response.data.message);
          }
        })
        .catch(error => {
          console.error('批量删除评论请求错误:', error);
          alert('批量删除评论失败，请稍后重试');
        });
    },
    
    batchApprove() {
      if (this.selectedComments.length === 0) return;
      
      axios.post('/api/admin/comments/batch-approve', { comment_ids: this.selectedComments })
        .then(response => {
          if (response.data.code === 0) {
            // 重新加载评论列表
            this.loadComments();
          } else {
            console.error('批量批准评论失败:', response.data.message);
            alert('批量批准评论失败: ' + response.data.message);
          }
        })
        .catch(error => {
          console.error('批量批准评论请求错误:', error);
          alert('批量批准评论失败，请稍后重试');
        });
    },
    
    getEmptyStateMessage() {
      if (this.isLoading) {
        return '正在加载评论...';
      }
      
      if (this.searchQuery.trim()) {
        return '没有符合搜索条件的评论';
      }
      
      switch (this.filters.status) {
        case 'approved':
          return '暂无已批准的评论';
        case 'pending':
          return '暂无待审核的评论';
        default:
          return '暂无任何评论';
      }
    }
  }
}
</script>

<style scoped>
.comments-view {
  padding: 0 16px;
}

.filter-bar {
  display: flex;
  justify-content: space-between;
  margin-bottom: 20px;
}

.filter-group {
  display: flex;
  gap: 10px;
}

.filter-select {
  padding: 8px 12px;
  border: 1px solid #ddd;
  border-radius: 4px;
  background-color: #fff;
  cursor: pointer;
}

.search-box {
  display: flex;
  align-items: center;
  border: 1px solid #ddd;
  border-radius: 4px;
  overflow: hidden;
}

.search-input {
  padding: 8px 12px;
  border: none;
  outline: none;
  width: 200px;
}

.search-btn {
  background-color: #f5f5f5;
  border: none;
  padding: 8px 12px;
  cursor: pointer;
}

.search-icon {
  font-size: 1rem;
}

.batch-actions {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 20px;
  padding: 10px;
  background-color: #f8f9fa;
  border-radius: 4px;
}

.batch-info {
  margin-right: auto;
  font-size: 0.9rem;
  color: #666;
}

.batch-btn {
  padding: 6px 12px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.batch-btn.approve {
  background-color: #d4edda;
  color: #155724;
}

.batch-btn.approve:hover {
  background-color: #c3e6cb;
}

.batch-btn.delete {
  background-color: #f8d7da;
  color: #721c24;
}

.batch-btn.delete:hover {
  background-color: #f5c6cb;
}

.table-container {
  background-color: #fff;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  overflow: hidden;
  margin-bottom: 20px;
}

.data-table {
  width: 100%;
  border-collapse: collapse;
}

.data-table th,
.data-table td {
  padding: 12px 16px;
  text-align: left;
  border-bottom: 1px solid #eee;
}

.data-table th {
  background-color: #f8f9fa;
  font-weight: 600;
  font-size: 0.9rem;
  color: #495057;
}

.th-checkbox, .td-checkbox {
  width: 30px;
}

.th-author, .td-author {
  width: 150px;
}

.th-comment, .td-comment {
  min-width: 300px;
}

.th-article, .td-article {
  width: 200px;
}

.th-date, .td-date {
  width: 180px;
}

.th-actions, .td-actions {
  width: 120px;
  text-align: center;
}

.author-info {
  display: flex;
  flex-direction: column;
}

.author-name {
  font-weight: 500;
}

.author-email {
  font-size: 0.8rem;
  color: #6c757d;
  margin-top: 4px;
}

.comment-content {
  line-height: 1.6;
}

.reply-info {
  margin-top: 6px;
  font-size: 0.8rem;
  color: #6c757d;
  font-style: italic;
}

.article-link {
  color: #333;
  text-decoration: none;
  transition: color 0.2s;
}

.article-link:hover {
  color: #3498db;
}

.action-buttons {
  display: flex;
  justify-content: center;
  gap: 8px;
}

.action-btn {
  background: none;
  border: none;
  font-size: 1.1rem;
  cursor: pointer;
  opacity: 0.7;
  transition: opacity 0.2s;
}

.action-btn:hover {
  opacity: 1;
}

.action-btn.reply:hover {
  color: #3498db;
}

.action-btn.approve:hover {
  color: #28a745;
}

.action-btn.delete:hover {
  color: #dc3545;
}

.table-row.selected {
  background-color: #f0f7fd;
}

.empty-state {
  padding: 40px;
  text-align: center;
}

.empty-icon {
  font-size: 3rem;
  margin-bottom: 20px;
  opacity: 0.5;
}

.empty-state h3 {
  margin-bottom: 10px;
  font-weight: 600;
}

.empty-state p {
  color: #6c757d;
}

.pagination {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-top: 30px;
}

.page-btn {
  padding: 6px 12px;
  border: 1px solid #ddd;
  background-color: #fff;
  cursor: pointer;
  transition: background-color 0.2s;
}

.page-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.page-numbers {
  display: flex;
  margin: 0 10px;
}

.page-number {
  padding: 6px 12px;
  border: 1px solid #ddd;
  background-color: #fff;
  margin: 0 5px;
  cursor: pointer;
}

.page-number.active {
  background-color: #3498db;
  color: white;
  border-color: #3498db;
}

.ellipsis {
  padding: 6px 12px;
}

/* 弹窗样式 */
.dialog-backdrop {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: rgba(0, 0, 0, 0.5);
  display: flex;
  justify-content: center;
  align-items: center;
  z-index: 1000;
}

.dialog {
  background-color: #fff;
  border-radius: 8px;
  width: 90%;
  max-width: 500px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
  overflow: hidden;
}

.dialog-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px;
  border-bottom: 1px solid #eee;
}

.dialog-header h3 {
  margin: 0;
  font-size: 1.2rem;
}

.close-btn {
  background: none;
  border: none;
  font-size: 1.5rem;
  cursor: pointer;
  opacity: 0.6;
  transition: opacity 0.2s;
}

.close-btn:hover {
  opacity: 1;
}

.dialog-body {
  padding: 16px;
}

.original-comment {
  background-color: #f8f9fa;
  padding: 12px;
  border-radius: 4px;
  margin-bottom: 16px;
}

.comment-author {
  font-weight: 600;
  margin-bottom: 6px;
}

.comment-text {
  white-space: pre-wrap;
}

.reply-textarea {
  width: 100%;
  padding: 10px;
  border: 1px solid #ddd;
  border-radius: 4px;
  resize: vertical;
}

.dialog-footer {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
  padding: 16px;
  border-top: 1px solid #eee;
}

.cancel-btn {
  padding: 8px 16px;
  background-color: #e9ecef;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

.submit-btn {
  padding: 8px 16px;
  background-color: #3498db;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

.submit-btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.delete-btn {
  padding: 8px 16px;
  background-color: #dc3545;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

.comment-preview {
  background-color: #f8f9fa;
  padding: 12px;
  border-radius: 4px;
  margin-top: 12px;
}

@media (max-width: 992px) {
  .th-article, .td-article,
  .th-date, .td-date {
    display: none;
  }
}

@media (max-width: 768px) {
  .filter-bar {
    flex-direction: column;
    gap: 10px;
  }
  
  .search-box {
    width: 100%;
  }
  
  .search-input {
    width: 100%;
  }
  
  .action-buttons {
    flex-direction: column;
    gap: 5px;
  }
}
</style> 