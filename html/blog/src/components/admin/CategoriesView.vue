<template>
  <div class="categories-view">
    <div class="card">
      <div class="card-header">
        <h3>分类管理</h3>
        <button class="add-btn" @click="showAddDialog">
          <span class="btn-icon">+</span>
          <span class="btn-text">新建分类</span>
        </button>
      </div>
      
      <div class="table-container">
        <table class="data-table" v-if="categories.length > 0">
          <thead>
            <tr>
              <th class="th-name">分类名称</th>
              <th class="th-slug">别名</th>
              <th class="th-count">文章数</th>
              <th class="th-actions">操作</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="category in categories" :key="category.id" class="table-row">
              <td class="td-name">
                <router-link :to="`/category/${category.slug}`" class="category-name">
                  {{ category.name }}
                </router-link>
              </td>
              <td class="td-slug">{{ category.slug }}</td>
              <td class="td-count">{{ getArticleCount(category.id) }}</td>
              <td class="td-actions">
                <div class="action-buttons">
                  <button @click="editCategory(category)" class="action-btn edit" title="编辑">
                    ✏️
                  </button>
                  <button 
                    @click="confirmDeleteCategory(category)" 
                    class="action-btn delete" 
                    title="删除"
                    :disabled="getArticleCount(category.id) > 0"
                  >
                    🗑️
                  </button>
                </div>
              </td>
            </tr>
          </tbody>
        </table>
        <div v-else class="empty-state">
          <div class="empty-icon">🏷️</div>
          <h3>暂无分类</h3>
          <p>创建分类可以更好地组织您的文章</p>
          <button class="create-category-btn" @click="showAddDialog">创建第一个分类</button>
        </div>
      </div>
    </div>
    
    <!-- 添加/编辑分类弹窗 -->
    <div v-if="dialogVisible" class="dialog-backdrop">
      <div class="dialog">
        <div class="dialog-header">
          <h3>{{ isEditing ? '编辑分类' : '新建分类' }}</h3>
          <button class="close-btn" @click="closeDialog">×</button>
        </div>
        <div class="dialog-body">
          <div class="form-group">
            <label for="category-name">分类名称</label>
            <input 
              type="text" 
              id="category-name" 
              v-model="formData.name"
              class="form-input"
              placeholder="请输入分类名称"
              required
            >
          </div>
          
          <div class="form-group">
            <label for="category-slug">分类别名</label>
            <div class="slug-input-container">
              <input 
                type="text" 
                id="category-slug" 
                v-model="formData.slug"
                class="form-input"
                placeholder="用于URL的别名，留空将自动生成"
              >
              <button @click="generateSlug" class="generate-slug-btn" title="自动生成别名">⟳</button>
            </div>
            <small class="form-hint">别名将用于构建URL，例如: /category/your-slug</small>
          </div>
          
          <div class="form-group">
            <label for="category-description">描述（可选）</label>
            <textarea 
              id="category-description" 
              v-model="formData.description"
              class="form-textarea"
              placeholder="输入分类的简短描述"
              rows="3"
            ></textarea>
          </div>
        </div>
        <div class="dialog-footer">
          <button class="cancel-btn" @click="closeDialog">取消</button>
          <button 
            class="submit-btn" 
            @click="submitCategory"
            :disabled="!formData.name.trim()"
          >
            {{ isEditing ? '保存修改' : '创建分类' }}
          </button>
        </div>
      </div>
    </div>
    
    <!-- 删除确认弹窗 -->
    <div v-if="deleteDialogVisible" class="dialog-backdrop">
      <div class="dialog">
        <div class="dialog-header">
          <h3>删除分类</h3>
          <button class="close-btn" @click="closeDeleteDialog">×</button>
        </div>
        <div class="dialog-body">
          <p>确定要删除分类 "{{ currentCategory?.name }}" 吗？此操作无法撤销。</p>
          <div v-if="getArticleCount(currentCategory?.id) > 0" class="warning-message">
            <strong>无法删除:</strong> 该分类下仍有 {{ getArticleCount(currentCategory?.id) }} 篇文章。请先将这些文章移至其他分类或删除。
          </div>
        </div>
        <div class="dialog-footer">
          <button class="cancel-btn" @click="closeDeleteDialog">取消</button>
          <button 
            class="delete-btn" 
            @click="deleteCategory"
            :disabled="getArticleCount(currentCategory?.id) > 0"
          >
            确认删除
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';

export default {
  name: 'CategoriesView',
  props: {
    articles: {
      type: Array,
      required: true
    }
  },
  data() {
    return {
      categories: [],
      dialogVisible: false,
      deleteDialogVisible: false,
      isEditing: false,
      formData: {
        name: '',
        slug: '',
        description: ''
      },
      currentCategory: null,
      isLoading: false
    };
  },
  computed: {
    // 用于生成友好的 URL 别名
    slugify(text) {
      return function(text) {
        return text
          .toString()
          .toLowerCase()
          .trim()
          .replace(/\s+/g, '-')        // 替换空格为连字符
          .replace(/[^\w\-]+/g, '')    // 移除非字母数字字符
          .replace(/\-\-+/g, '-')      // 替换多个连字符为单个
          .replace(/^-+/, '')          // 去除开头的连字符
          .replace(/-+$/, '');         // 去除结尾的连字符
      };
    }
  },
  mounted() {
    this.loadCategories();
  },
  methods: {
    loadCategories() {
      this.isLoading = true;
      
      axios.get('/api/categories')
        .then(response => {
          if (response.data.code === 0) {
            this.categories = response.data.data || [];
          } else {
            console.error('获取分类失败:', response.data.message);
            this.categories = [];
          }
        })
        .catch(error => {
          console.error('获取分类请求错误:', error);
          this.categories = [];
        })
        .finally(() => {
          this.isLoading = false;
        });
    },
    
    getArticleCount(categoryId) {
      // 统计属于该分类的文章数量
      return this.articles.filter(article => {
        return article.categories && 
          article.categories.some(cat => cat.id === categoryId);
      }).length;
    },
    
    showAddDialog() {
      this.isEditing = false;
      this.formData = {
        name: '',
        slug: '',
        description: ''
      };
      this.dialogVisible = true;
    },
    
    editCategory(category) {
      this.isEditing = true;
      this.currentCategory = category;
      this.formData = {
        name: category.name,
        slug: category.slug || '',
        description: category.description || ''
      };
      this.dialogVisible = true;
    },
    
    closeDialog() {
      this.dialogVisible = false;
      this.currentCategory = null;
    },
    
    generateSlug() {
      if (this.formData.name.trim()) {
        this.formData.slug = this.slugify(this.formData.name);
      }
    },
    
    submitCategory() {
      if (!this.formData.name.trim()) return;
      
      const data = {
        name: this.formData.name.trim(),
        slug: this.formData.slug.trim(),
        description: this.formData.description.trim()
      };
      
      // 如果别名为空，自动生成
      if (!data.slug) {
        data.slug = this.slugify(data.name);
      }
      
      if (this.isEditing && this.currentCategory) {
        // 更新分类
        axios.put(`/api/categories/${this.currentCategory.id}`, data)
          .then(response => {
            if (response.data.code === 0) {
              this.closeDialog();
              this.loadCategories();
            } else {
              console.error('更新分类失败:', response.data.message);
              alert('更新分类失败: ' + response.data.message);
            }
          })
          .catch(error => {
            console.error('更新分类请求错误:', error);
            alert('更新分类失败，请稍后重试');
          });
      } else {
        // 新建分类
        axios.post('/api/categories', data)
          .then(response => {
            if (response.data.code === 0) {
              this.closeDialog();
              this.loadCategories();
            } else {
              console.error('创建分类失败:', response.data.message);
              alert('创建分类失败: ' + response.data.message);
            }
          })
          .catch(error => {
            console.error('创建分类请求错误:', error);
            alert('创建分类失败，请稍后重试');
          });
      }
    },
    
    confirmDeleteCategory(category) {
      this.currentCategory = category;
      this.deleteDialogVisible = true;
    },
    
    closeDeleteDialog() {
      this.deleteDialogVisible = false;
      this.currentCategory = null;
    },
    
    deleteCategory() {
      if (!this.currentCategory) return;
      
      // 检查是否有文章使用该分类
      if (this.getArticleCount(this.currentCategory.id) > 0) {
        alert('该分类下仍有文章，无法删除');
        return;
      }
      
      axios.delete(`/api/categories/${this.currentCategory.id}`)
        .then(response => {
          if (response.data.code === 0) {
            this.closeDeleteDialog();
            // 从列表中移除已删除的分类
            this.categories = this.categories.filter(c => c.id !== this.currentCategory.id);
          } else {
            console.error('删除分类失败:', response.data.message);
            alert('删除分类失败: ' + response.data.message);
          }
        })
        .catch(error => {
          console.error('删除分类请求错误:', error);
          alert('删除分类失败，请稍后重试');
        });
    }
  }
}
</script>

<style scoped>
.categories-view {
  padding: 0 16px;
}

.card {
  background-color: #fff;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  overflow: hidden;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 24px;
  border-bottom: 1px solid #eee;
}

.card-header h3 {
  margin: 0;
  font-size: 1.2rem;
  font-weight: 600;
}

.add-btn {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 8px 16px;
  background-color: #3498db;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.add-btn:hover {
  background-color: #2980b9;
}

.btn-icon {
  font-size: 1.1rem;
}

.table-container {
  padding: 0 16px 16px;
}

.data-table {
  width: 100%;
  border-collapse: collapse;
  margin-top: 16px;
}

.data-table th,
.data-table td {
  padding: 12px 16px;
  text-align: left;
  border-bottom: 1px solid #eee;
}

.data-table th {
  font-weight: 600;
  font-size: 0.9rem;
  color: #495057;
}

.th-name, .td-name {
  width: 30%;
}

.th-slug, .td-slug {
  width: 30%;
  color: #6c757d;
}

.th-count, .td-count {
  width: 20%;
  text-align: center;
}

.th-actions, .td-actions {
  width: 20%;
  text-align: center;
}

.category-name {
  color: #333;
  text-decoration: none;
  font-weight: 500;
  transition: color 0.2s;
}

.category-name:hover {
  color: #3498db;
}

.action-buttons {
  display: flex;
  justify-content: center;
  gap: 12px;
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

.action-btn:disabled {
  opacity: 0.3;
  cursor: not-allowed;
}

.action-btn.edit:hover {
  color: #3498db;
}

.action-btn.delete:hover {
  color: #dc3545;
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
  margin-bottom: 16px;
}

.create-category-btn {
  display: inline-block;
  padding: 8px 16px;
  background-color: #3498db;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.create-category-btn:hover {
  background-color: #2980b9;
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

.form-group {
  margin-bottom: 16px;
}

.form-group label {
  display: block;
  margin-bottom: 6px;
  font-weight: 500;
  color: #444;
}

.form-input, .form-textarea {
  width: 100%;
  padding: 8px 12px;
  border: 1px solid #d1d5db;
  border-radius: 4px;
  font-size: 0.95rem;
}

.form-input:focus, .form-textarea:focus {
  outline: none;
  border-color: #3498db;
  box-shadow: 0 0 0 2px rgba(52, 152, 219, 0.25);
}

.form-textarea {
  resize: vertical;
}

.form-hint {
  display: block;
  margin-top: 6px;
  font-size: 0.85rem;
  color: #6c757d;
}

.slug-input-container {
  display: flex;
  align-items: center;
  gap: 8px;
}

.generate-slug-btn {
  padding: 8px;
  background-color: #f5f5f5;
  border: 1px solid #d1d5db;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.generate-slug-btn:hover {
  background-color: #e9ecef;
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

.delete-btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.warning-message {
  margin-top: 12px;
  padding: 12px;
  background-color: #fff3cd;
  border-radius: 4px;
  color: #856404;
}

@media (max-width: 768px) {
  .th-slug, .td-slug {
    display: none;
  }
  
  .card-header {
    flex-direction: column;
    align-items: flex-start;
    gap: 12px;
  }
  
  .add-btn {
    align-self: stretch;
    justify-content: center;
  }
}
</style> 