<template>
  <div class="comment-item">
    <div class="comment-info">
      <div class="comment-author">{{ comment.author }}</div>
      <div class="comment-date">{{ formatDate(comment.created_at) }}</div>
    </div>
    
    <!-- 引用区域 - 显示回复的评论内容 -->
    <div v-if="hasParent" class="comment-quote">
      <div class="quote-author">@{{ comment.parent_author }}</div>
      <div class="quote-content">{{ comment.parent_content }}</div>
    </div>
    
    <div class="comment-content">{{ comment.content }}</div>
    
    <div class="comment-actions">
      <button class="action-btn reply-btn" @click="$emit('reply', comment)">回复</button>
      <button 
        v-if="isAdmin || (isLoggedIn && comment.user_id === getUserId())" 
        class="action-btn delete-btn" 
        @click="$emit('delete', comment.id)"
      >
        删除
      </button>
    </div>
  </div>
</template>

<script>
export default {
  name: 'CommentItem',
  props: {
    comment: {
      type: Object,
      required: true
    },
    articleId: {
      type: Number,
      required: true
    }
  },
  
  computed: {
    isLoggedIn() {
      return localStorage.getItem('isLoggedIn') === 'true';
    },
    
    isAdmin() {
      const role = localStorage.getItem('userRole');
      return role === 'admin';
    },
    
    hasParent() {
      return this.comment.parent_id && 
             this.comment.parent_id > 0 && 
             this.comment.parent_author && 
             this.comment.parent_content;
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
    
    getUserId() {
      const userId = localStorage.getItem('userId');
      return userId ? parseInt(userId, 10) : 0;
    }
  }
};
</script>

<style scoped>
.comment-item {
  padding: 15px;
  margin-bottom: 20px;
  border: 1px solid #eee;
  border-radius: 5px;
  background-color: #fff;
}

.comment-info {
  display: flex;
  justify-content: space-between;
  margin-bottom: 10px;
}

.comment-author {
  font-weight: bold;
  color: #333;
}

.comment-date {
  font-size: 0.8rem;
  color: #777;
}

/* 引用样式 */
.comment-quote {
  margin: 10px 0;
  padding: 10px;
  background-color: #f8f8f8;
  border-left: 3px solid #ddd;
  border-radius: 0 3px 3px 0;
}

.quote-author {
  font-weight: bold;
  color: #3498db;
  margin-bottom: 5px;
}

.quote-content {
  color: #666;
  font-size: 0.9rem;
  white-space: pre-line;
}

.comment-content {
  line-height: 1.5;
  margin: 12px 0;
  word-break: break-word;
  white-space: pre-line;
}

.comment-actions {
  display: flex;
  gap: 10px;
  margin-top: 10px;
}

.action-btn {
  background: none;
  border: none;
  font-size: 0.9rem;
  cursor: pointer;
  padding: 3px 8px;
  border-radius: 3px;
}

.reply-btn {
  color: #3498db;
}

.reply-btn:hover {
  background-color: #edf7fd;
}

.delete-btn {
  color: #e74c3c;
}

.delete-btn:hover {
  background-color: #fdedec;
}
</style> 