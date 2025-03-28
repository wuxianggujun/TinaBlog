<template>
  <div class="profile-container">
    <div class="profile-header">
      <h1>个人信息</h1>
      <p>查看和修改您的个人资料</p>
    </div>
    
    <div class="content-card" v-if="!isLoading && !error">
      <div class="profile-tabs">
        <button 
          @click="activeTab = 'info'" 
          :class="['tab-btn', { active: activeTab === 'info' }]"
        >
          基本信息
        </button>
        <button 
          @click="activeTab = 'security'" 
          :class="['tab-btn', { active: activeTab === 'security' }]"
        >
          账号安全
        </button>
        <button 
          @click="activeTab = 'social'" 
          :class="['tab-btn', { active: activeTab === 'social' }]"
        >
          私密信息
        </button>
      </div>
      
      <!-- 基本信息标签页 -->
      <div v-if="activeTab === 'info'" class="tab-content">
        <div class="avatar-section">
          <div class="avatar-container">
            <div class="avatar">
              <span v-if="!userInfo.avatar" class="avatar-placeholder">{{ userInfo.display_name?.[0] || userInfo.username?.[0] || '用' }}</span>
              <img v-else :src="userInfo.avatar" alt="头像" />
            </div>
            <button class="avatar-upload-btn" @click="triggerFileUpload">
              <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="18" height="18" fill="currentColor">
                <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm5 11h-4v4h-2v-4H7v-2h4V7h2v4h4v2z"/>
              </svg>
              更换头像
            </button>
            <input 
              type="file" 
              ref="fileInput" 
              style="display: none" 
              accept="image/*" 
              @change="handleAvatarUpload" 
            />
          </div>
        </div>
        
        <form @submit.prevent="updateProfile" class="profile-form">
          <div class="form-group">
            <label for="username">用户名</label>
            <input 
              type="text" 
              id="username" 
              v-model="userInfo.username" 
              class="form-input" 
              disabled
              title="用户名不可修改"
            >
          </div>
          
          <div class="form-group">
            <label for="display_name">昵称</label>
            <input 
              type="text" 
              id="display_name" 
              v-model="userInfo.display_name" 
              class="form-input"
              placeholder="请输入您的昵称"
            >
          </div>
          
          <div class="form-group">
            <label for="email">电子邮箱</label>
            <input 
              type="email" 
              id="email" 
              v-model="userInfo.email" 
              class="form-input"
              placeholder="请输入您的电子邮箱"
            >
          </div>
          
          <div class="form-group">
            <label for="bio">个人简介</label>
            <textarea 
              id="bio" 
              v-model="userInfo.bio" 
              class="form-textarea"
              placeholder="介绍一下您自己..."
              rows="4"
            ></textarea>
          </div>
          
          <div class="form-actions">
            <button type="submit" class="btn btn-primary" :disabled="isSaving">
              {{ isSaving ? '保存中...' : '保存修改' }}
            </button>
          </div>
        </form>
        
        <!-- 成功或错误提示 -->
        <div v-if="updateSuccess" class="alert alert-success">
          个人信息更新成功！
        </div>
        <div v-if="updateError" class="alert alert-error">
          {{ updateErrorMessage }}
        </div>
      </div>
      
      <!-- 账号安全标签页 -->
      <div v-if="activeTab === 'security'" class="tab-content">
        <form @submit.prevent="changePassword" class="profile-form">
          <div class="form-group">
            <label for="current_password">当前密码</label>
            <input 
              type="password" 
              id="current_password" 
              v-model="passwordForm.currentPassword" 
              class="form-input"
              placeholder="请输入当前密码"
              required
            >
          </div>
          
          <div class="form-group">
            <label for="new_password">新密码</label>
            <input 
              type="password" 
              id="new_password" 
              v-model="passwordForm.newPassword" 
              class="form-input"
              placeholder="请输入新密码"
              required
              minlength="8"
            >
          </div>
          
          <div class="form-group">
            <label for="confirm_password">确认新密码</label>
            <input 
              type="password" 
              id="confirm_password" 
              v-model="passwordForm.confirmPassword" 
              class="form-input"
              placeholder="请再次输入新密码"
              required
            >
            <span v-if="passwordForm.newPassword && passwordForm.confirmPassword && passwordForm.newPassword !== passwordForm.confirmPassword" class="error-text">
              两次输入的密码不一致
            </span>
          </div>
          
          <div class="form-actions">
            <button 
              type="submit" 
              class="btn btn-primary" 
              :disabled="isChangingPassword || !passwordForm.currentPassword || !passwordForm.newPassword || !passwordForm.confirmPassword || (passwordForm.newPassword !== passwordForm.confirmPassword)"
            >
              {{ isChangingPassword ? '处理中...' : '修改密码' }}
            </button>
          </div>
        </form>
        
        <!-- 成功或错误提示 -->
        <div v-if="passwordChangeSuccess" class="alert alert-success">
          密码修改成功！
        </div>
        <div v-if="passwordChangeError" class="alert alert-error">
          {{ passwordChangeErrorMessage }}
        </div>
      </div>
      
      <!-- 私密信息标签页 -->
      <div v-if="activeTab === 'social'" class="tab-content">
        <form @submit.prevent="updateSocialLinks" class="profile-form">
          <div class="form-group">
            <label for="github">GitHub 链接</label>
            <input 
              type="url" 
              id="github" 
              v-model="socialLinks.github" 
              class="form-input"
              placeholder="https://github.com/您的用户名"
            >
          </div>
          
          <div class="form-group">
            <label for="website">个人网站</label>
            <input 
              type="url" 
              id="website" 
              v-model="socialLinks.website" 
              class="form-input"
              placeholder="https://您的网站.com"
            >
          </div>
          
          <div class="form-group">
            <label for="twitter">Twitter/X 链接</label>
            <input 
              type="url" 
              id="twitter" 
              v-model="socialLinks.twitter" 
              class="form-input"
              placeholder="https://twitter.com/您的用户名"
            >
          </div>
          
          <div class="form-group">
            <label for="weibo">微博链接</label>
            <input 
              type="url" 
              id="weibo" 
              v-model="socialLinks.weibo" 
              class="form-input"
              placeholder="https://weibo.com/您的用户名"
            >
          </div>
          
          <div class="form-group">
            <label for="linkedin">LinkedIn 链接</label>
            <input 
              type="url" 
              id="linkedin" 
              v-model="socialLinks.linkedin" 
              class="form-input"
              placeholder="https://www.linkedin.com/in/您的用户名"
            >
          </div>
          
          <div class="form-group">
            <label for="contact_email">联系邮箱</label>
            <input 
              type="email" 
              id="contact_email" 
              v-model="socialLinks.contactEmail" 
              class="form-input"
              placeholder="public@example.com"
            >
            <small class="form-hint">此邮箱将公开显示在您的个人主页上</small>
          </div>
          
          <div class="form-actions">
            <button type="submit" class="btn btn-primary" :disabled="isSavingSocial">
              {{ isSavingSocial ? '保存中...' : '保存修改' }}
            </button>
          </div>
        </form>
        
        <!-- 成功或错误提示 -->
        <div v-if="socialUpdateSuccess" class="alert alert-success">
          社交链接更新成功！
        </div>
        <div v-if="socialUpdateError" class="alert alert-error">
          {{ socialUpdateErrorMessage }}
        </div>
      </div>
    </div>
    
    <!-- 加载中状态 -->
    <div v-if="isLoading" class="loading">
      <p>正在加载用户信息...</p>
    </div>
    
    <!-- 错误状态 -->
    <div v-if="error" class="error">
      <p>{{ errorMessage }}</p>
      <button @click="fetchUserInfo" class="btn">重试</button>
    </div>
    
    <!-- 删除确认对话框 -->
    <div v-if="showDeleteConfirm" class="delete-confirm-modal">
      <div class="modal-content">
        <h3>确认删除</h3>
        <p>您确定要删除文章 "{{ articleToDelete?.title }}" 吗？此操作不可撤销。</p>
        <div class="modal-actions">
          <button @click="deleteArticle" class="btn btn-danger" :disabled="isDeleting">
            {{ isDeleting ? '删除中...' : '确认删除' }}
          </button>
          <button @click="cancelDelete" class="btn btn-secondary">取消</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';

export default {
  name: 'ProfileView',
  props: {
    activeTab: {
      type: String,
      default: 'info'
    }
  },
  data() {
    return {
      activeTab: this.activeTab,
      userInfo: {
        username: '',
        display_name: '',
        email: '',
        bio: '',
        uuid: '',
        is_admin: false,
        avatar: ''
      },
      isLoading: true,
      error: false,
      errorMessage: '加载失败',
      
      // 更新用户信息相关
      isSaving: false,
      updateSuccess: false,
      updateError: false,
      updateErrorMessage: '',
      
      // 修改密码相关
      passwordForm: {
        currentPassword: '',
        newPassword: '',
        confirmPassword: ''
      },
      isChangingPassword: false,
      passwordChangeSuccess: false,
      passwordChangeError: false,
      passwordChangeErrorMessage: '',
      
      // 文章相关
      userArticles: [],
      loadingArticles: false,
      articlesError: false,
      articlesErrorMessage: '',
      currentArticlePage: 1,
      articlesPerPage: 10,
      totalArticles: 0,
      totalArticlePages: 0,
      
      // 删除文章相关
      showDeleteConfirm: false,
      articleToDelete: null,
      isDeleting: false,
      
      // 社交链接相关
      socialLinks: {
        github: '',
        website: '',
        twitter: '',
        weibo: '',
        linkedin: '',
        contactEmail: ''
      },
      isSavingSocial: false,
      socialUpdateSuccess: false,
      socialUpdateError: false,
      socialUpdateErrorMessage: ''
    };
  },
  created() {
    // 设置页面标题
    document.title = '个人信息 - Tina博客';
    this.fetchUserInfo();
  },
  watch: {
    activeTab(newTab) {
      if (newTab === 'articles' && this.userArticles.length === 0) {
        this.fetchUserArticles();
      }
    }
  },
  methods: {
    triggerFileUpload() {
      this.$refs.fileInput.click();
    },
    
    handleAvatarUpload(event) {
      const file = event.target.files[0];
      if (!file) return;
      
      // 验证文件类型
      if (!file.type.match('image.*')) {
        alert('请上传图片文件');
        return;
      }
      
      // 验证文件大小（限制为2MB）
      if (file.size > 2 * 1024 * 1024) {
        alert('图片大小不能超过2MB');
        return;
      }
      
      // 设置上传状态
      this.isSaving = true;
      
      // 创建FormData对象
      const formData = new FormData();
      formData.append('source', file);
      
      // 使用后端代理上传头像到图床
      axios.post('/api/auth/upload-image', formData, {
        headers: {
          'Content-Type': 'multipart/form-data'
        }
      })
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            // 后端已经完成了图片上传和用户资料更新
            const data = response.data.data;
            
            // 更新本地头像URL
            this.userInfo.avatar = data.image_url;
            
            // 显示成功消息
            this.updateSuccess = true;
            setTimeout(() => {
              this.updateSuccess = false;
            }, 3000);
          } else {
            throw new Error(response.data.message || '头像上传失败');
          }
        })
        .catch(error => {
          this.updateError = true;
          this.updateErrorMessage = error.message || '网络错误，请稍后重试';
          console.error('头像上传失败:', error);
        })
        .finally(() => {
          this.isSaving = false;
          // 清空文件输入框，允许再次选择同一文件
          this.$refs.fileInput.value = '';
        });
    },
    
    formatDate(dateString) {
      if (!dateString) return '';
      
      const options = { year: 'numeric', month: 'long', day: 'numeric', hour: '2-digit', minute: '2-digit' };
      return new Date(dateString).toLocaleDateString('zh-CN', options);
    },
    
    fetchUserInfo() {
      this.isLoading = true;
      this.error = false;
      
      axios.get('/api/auth/info')
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.userInfo = {
              ...this.userInfo,
              ...response.data.data
            };
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取用户信息失败';
          }
        })
        .catch(error => {
          this.error = true;
          this.errorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('获取用户信息失败:', error);
          
          // 如果是401错误，跳转到登录页
          if (error.response && error.response.status === 401) {
            localStorage.removeItem('isLoggedIn');
            localStorage.removeItem('token');
            localStorage.removeItem('username');
            this.$router.push('/login?redirect=/profile');
          }
        })
        .finally(() => {
          this.isLoading = false;
        });
    },
    
    updateProfile() {
      this.isSaving = true;
      this.updateSuccess = false;
      this.updateError = false;
      
      const updateData = {
        display_name: this.userInfo.display_name,
        email: this.userInfo.email,
        bio: this.userInfo.bio
      };
      
      axios.put('/api/auth/profile', updateData)
        .then(response => {
          if (response.data.code === 0) {
            this.updateSuccess = true;
            
            // 3秒后隐藏成功提示
            setTimeout(() => {
              this.updateSuccess = false;
            }, 3000);
          } else {
            this.updateError = true;
            this.updateErrorMessage = response.data.message || '更新个人信息失败';
          }
        })
        .catch(error => {
          this.updateError = true;
          this.updateErrorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('更新个人信息失败:', error);
        })
        .finally(() => {
          this.isSaving = false;
        });
    },
    
    changePassword() {
      // 验证密码
      if (this.passwordForm.newPassword !== this.passwordForm.confirmPassword) {
        this.passwordChangeError = true;
        this.passwordChangeErrorMessage = '两次输入的密码不一致';
        return;
      }
      
      this.isChangingPassword = true;
      this.passwordChangeSuccess = false;
      this.passwordChangeError = false;
      
      const passwordData = {
        current_password: this.passwordForm.currentPassword,
        new_password: this.passwordForm.newPassword
      };
      
      axios.post('/api/auth/change-password', passwordData)
        .then(response => {
          if (response.data.code === 0) {
            this.passwordChangeSuccess = true;
            
            // 清空表单
            this.passwordForm.currentPassword = '';
            this.passwordForm.newPassword = '';
            this.passwordForm.confirmPassword = '';
            
            // 3秒后隐藏成功提示
            setTimeout(() => {
              this.passwordChangeSuccess = false;
            }, 3000);
          } else {
            this.passwordChangeError = true;
            this.passwordChangeErrorMessage = response.data.message || '修改密码失败';
          }
        })
        .catch(error => {
          this.passwordChangeError = true;
          this.passwordChangeErrorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('修改密码失败:', error);
        })
        .finally(() => {
          this.isChangingPassword = false;
        });
    },
    
    fetchUserArticles() {
      this.loadingArticles = true;
      this.articlesError = false;
      
      axios.get('/api/posts', {
        params: {
          page: this.currentArticlePage,
          pageSize: this.articlesPerPage
        }
      })
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.userArticles = response.data.data.articles || [];
            this.totalArticles = response.data.data.pagination?.total || 0;
            this.totalArticlePages = response.data.data.pagination?.totalPages || 0;
          } else {
            this.articlesError = true;
            this.articlesErrorMessage = response.data.message || '获取文章列表失败';
          }
        })
        .catch(error => {
          this.articlesError = true;
          this.articlesErrorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('获取文章列表失败:', error);
        })
        .finally(() => {
          this.loadingArticles = false;
        });
    },
    
    changePage(page) {
      if (page < 1 || page > this.totalArticlePages) return;
      
      this.currentArticlePage = page;
      this.fetchUserArticles();
    },
    
    confirmDeleteArticle(article) {
      this.articleToDelete = article;
      this.showDeleteConfirm = true;
    },
    
    deleteArticle() {
      if (!this.articleToDelete) return;
      
      this.isDeleting = true;
      
      axios.delete(`/api/posts/${this.articleToDelete.id}`)
        .then(response => {
          if (response.data.code === 0) {
            // 从列表中移除已删除的文章
            this.userArticles = this.userArticles.filter(article => article.id !== this.articleToDelete.id);
            this.cancelDelete();
            
            // 如果当前页已经没有文章且不是第一页，回到上一页
            if (this.userArticles.length === 0 && this.currentArticlePage > 1) {
              this.changePage(this.currentArticlePage - 1);
            }
          } else {
            alert(response.data.message || '删除文章失败');
          }
        })
        .catch(error => {
          alert(error.response?.data?.message || '网络错误，请稍后重试');
          console.error('删除文章失败:', error);
        })
        .finally(() => {
          this.isDeleting = false;
        });
    },
    
    cancelDelete() {
      this.showDeleteConfirm = false;
      this.articleToDelete = null;
    },
    
    updateSocialLinks() {
      this.isSavingSocial = true;
      this.socialUpdateSuccess = false;
      this.socialUpdateError = false;
      
      const updateData = {
        github: this.socialLinks.github,
        website: this.socialLinks.website,
        twitter: this.socialLinks.twitter,
        weibo: this.socialLinks.weibo,
        linkedin: this.socialLinks.linkedin,
        contactEmail: this.socialLinks.contactEmail
      };
      
      axios.put('/api/auth/social-links', updateData)
        .then(response => {
          if (response.data.code === 0) {
            this.socialUpdateSuccess = true;
            
            // 3秒后隐藏成功提示
            setTimeout(() => {
              this.socialUpdateSuccess = false;
            }, 3000);
          } else {
            this.socialUpdateError = true;
            this.socialUpdateErrorMessage = response.data.message || '更新社交链接失败';
          }
        })
        .catch(error => {
          this.socialUpdateError = true;
          this.socialUpdateErrorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('更新社交链接失败:', error);
        })
        .finally(() => {
          this.isSavingSocial = false;
        });
    }
  }
}
</script>

<style scoped>
.profile-container {
  max-width: 1000px;
  margin: 2rem auto;
  padding: 0 1.5rem;
}

.profile-header {
  text-align: center;
  margin-bottom: 2rem;
  padding: 1.5rem;
  background: linear-gradient(135deg, #6366f1 0%, #8b5cf6 100%);
  color: white;
  border-radius: 10px;
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
}

.profile-header h1 {
  font-size: 2.5rem;
  margin-bottom: 0.5rem;
}

.profile-header p {
  font-size: 1.1rem;
  opacity: 0.9;
}

.content-card {
  background: white;
  border-radius: 10px;
  padding: 2rem;
  box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
}

.profile-tabs {
  display: flex;
  border-bottom: 1px solid #e0e0e0;
  margin-bottom: 2rem;
  gap: 0.5rem;
}

.tab-btn {
  background: none;
  border: none;
  padding: 1rem 1.5rem;
  font-size: 1rem;
  font-weight: 500;
  color: #666;
  cursor: pointer;
  position: relative;
  transition: color 0.3s;
}

.tab-btn:hover {
  color: #6366f1;
}

.tab-btn.active {
  color: #6366f1;
  font-weight: 600;
}

.tab-btn.active::after {
  content: '';
  position: absolute;
  bottom: -1px;
  left: 0;
  right: 0;
  height: 3px;
  background-color: #6366f1;
  border-top-left-radius: 3px;
  border-top-right-radius: 3px;
}

.tab-content {
  padding: 1rem 0;
}

/* 头像相关样式 */
.avatar-section {
  display: flex;
  justify-content: center;
  margin-bottom: 2rem;
}

.avatar-container {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 1rem;
}

.avatar {
  width: 120px;
  height: 120px;
  border-radius: 50%;
  overflow: hidden;
  background-color: #e0e0e0;
  display: flex;
  justify-content: center;
  align-items: center;
  border: 3px solid #f1f1f1;
  box-shadow: 0 3px 10px rgba(0, 0, 0, 0.1);
}

.avatar img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.avatar-placeholder {
  font-size: 3rem;
  color: #6366f1;
  font-weight: bold;
}

.avatar-upload-btn {
  background-color: #f5f5f5;
  color: #555;
  border: none;
  border-radius: 20px;
  padding: 0.5rem 1rem;
  font-size: 0.9rem;
  cursor: pointer;
  display: flex;
  align-items: center;
  gap: 0.5rem;
  transition: background-color 0.2s;
}

.avatar-upload-btn:hover {
  background-color: #e0e0e0;
}

.profile-form {
  max-width: 600px;
  margin: 0 auto;
}

.form-group {
  margin-bottom: 1.5rem;
}

.form-group label {
  display: block;
  margin-bottom: 0.5rem;
  font-weight: 500;
  color: #444;
}

.form-input, .form-textarea {
  width: 100%;
  padding: 0.75rem 1rem;
  border: 1px solid #d1d5db;
  border-radius: 5px;
  font-size: 1rem;
  transition: border-color 0.2s;
}

.form-input:focus, .form-textarea:focus {
  border-color: #6366f1;
  outline: none;
  box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.2);
}

.form-input::placeholder, .form-textarea::placeholder {
  color: #9ca3af;
}

.form-input:disabled {
  background-color: #f5f5f5;
  cursor: not-allowed;
}

.form-actions {
  margin-top: 2rem;
  text-align: center;
}

.btn {
  padding: 0.75rem 1.5rem;
  border: none;
  border-radius: 5px;
  font-size: 1rem;
  font-weight: 500;
  cursor: pointer;
  transition: background-color 0.2s;
}

.btn-primary {
  background-color: #6366f1;
  color: white;
}

.btn-primary:hover {
  background-color: #4f46e5;
}

.btn-primary:disabled {
  background-color: #a5a6f6;
  cursor: not-allowed;
}

.btn-secondary {
  background-color: #e0e0e0;
  color: #333;
}

.btn-secondary:hover {
  background-color: #d0d0d0;
}

.btn-danger {
  background-color: #ef4444;
  color: white;
}

.btn-danger:hover {
  background-color: #dc2626;
}

.btn-small {
  padding: 0.4rem 0.75rem;
  font-size: 0.875rem;
}

.alert {
  padding: 1rem;
  border-radius: 5px;
  margin-top: 1.5rem;
}

.alert-success {
  background-color: #dcfce7;
  color: #166534;
  border: 1px solid #86efac;
}

.alert-error {
  background-color: #fee2e2;
  color: #991b1b;
  border: 1px solid #fca5a5;
}

.error-text {
  color: #dc2626;
  font-size: 0.875rem;
  margin-top: 0.5rem;
  display: block;
}

.loading, .error {
  text-align: center;
  padding: 3rem;
  background: white;
  border-radius: 10px;
  box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
}

/* 文章列表样式 */
.article-list {
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.article-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem;
  border: 1px solid #e0e0e0;
  border-radius: 5px;
  transition: transform 0.2s, box-shadow 0.2s;
  background-color: #f9fafb;
}

.article-item:hover {
  transform: translateY(-3px);
  box-shadow: 0 3px 10px rgba(0, 0, 0, 0.1);
}

.article-info {
  flex: 1;
}

.article-title {
  margin: 0 0 0.5rem;
  font-size: 1.25rem;
}

.article-title a {
  color: #333;
  text-decoration: none;
  transition: color 0.2s;
}

.article-title a:hover {
  color: #6366f1;
}

.article-meta {
  display: flex;
  align-items: center;
  gap: 1rem;
  font-size: 0.875rem;
  color: #666;
}

.article-status {
  padding: 0.25rem 0.5rem;
  border-radius: 20px;
  font-size: 0.75rem;
}

.article-status.published {
  background-color: #dcfce7;
  color: #166534;
}

.article-status.draft {
  background-color: #f3f4f6;
  color: #4b5563;
}

.article-actions {
  display: flex;
  gap: 0.5rem;
}

.pagination {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-top: 2rem;
  gap: 1rem;
}

.page-info {
  font-size: 0.9rem;
  color: #666;
}

.pagination-btn {
  padding: 0.5rem 1rem;
}

.loading-articles, .error-message {
  text-align: center;
  padding: 2rem 0;
  color: #666;
}

.empty-articles {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 3rem 0;
}

.empty-articles-content {
  text-align: center;
  background-color: #f9fafb;
  border-radius: 8px;
  padding: 2rem;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
}

.empty-articles p {
  color: #666;
  margin-bottom: 1.5rem;
  font-size: 1.1rem;
}

.empty-articles .btn {
  margin-top: 0.5rem;
}

/* 删除确认对话框 */
.delete-confirm-modal {
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

.modal-content {
  background: white;
  border-radius: 10px;
  padding: 2rem;
  max-width: 500px;
  width: 90%;
}

.modal-content h3 {
  margin-top: 0;
  color: #333;
}

.modal-actions {
  display: flex;
  justify-content: flex-end;
  gap: 1rem;
  margin-top: 2rem;
}

@media (max-width: 768px) {
  .profile-header h1 {
    font-size: 2rem;
  }
  
  .content-card {
    padding: 1.5rem;
  }
  
  .profile-tabs {
    justify-content: flex-start;
    overflow-x: auto;
    padding-bottom: 0.5rem;
  }
  
  .tab-btn {
    padding: 0.5rem 1rem;
    font-size: 0.9rem;
    white-space: nowrap;
  }
  
  .article-item {
    flex-direction: column;
    align-items: flex-start;
  }
  
  .article-actions {
    margin-top: 1rem;
    align-self: flex-end;
  }
  
  .avatar {
    width: 100px;
    height: 100px;
  }
}
</style> 