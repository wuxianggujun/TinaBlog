<template>
  <div class="comment-item" :class="{ 'is-reply': comment.parent_id }">
    <div class="comment-info">
      <div class="comment-author">{{ comment.author }}</div>
      <div class="comment-date">{{ formatDate(comment.created_at) }}</div>
    </div>
    
    <div class="comment-parent" v-if="comment.parent_author">
      <span>回复</span>
      <span class="parent-author">@{{ comment.parent_author }}:</span>
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
    
    <!-- 嵌套评论 -->
    <div class="child-comments" v-if="comment.replies && comment.replies.length > 0">
      <comment-item 
        v-for="reply in comment.replies" 
        :key="reply.id" 
        :comment="reply"
        :article-id="articleId"
        @reply="$emit('reply', $event)"
        @delete="$emit('delete', $event)"
      />
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

.comment-item.is-reply {
  margin-left: 30px;
  border-left: 3px solid #3498db;
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

.comment-parent {
  background-color: #f9f9f9;
  padding: 5px 10px;
  margin-bottom: 10px;
  border-radius: 3px;
  font-size: 0.9rem;
  color: #666;
}

.parent-author {
  font-weight: bold;
  color: #3498db;
}

.comment-content {
  line-height: 1.5;
  margin-bottom: 10px;
  word-break: break-word;
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

.child-comments {
  margin-top: 20px;
}
</style> 