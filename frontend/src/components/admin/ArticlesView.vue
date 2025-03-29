<template>
  <div class="articles-view">
    <div class="filter-bar">
      <div class="filter-group">
        <select v-model="filters.status" class="filter-select" @change="onFilterChange">
          <option value="all">所有状态</option>
          <option value="published">已发布</option>
          <option value="draft">草稿</option>
        </select>
        
        <select v-model="filters.sortBy" class="filter-select" @change="onFilterChange">
          <option value="newest">最新发布</option>
          <option value="oldest">最早发布</option>
          <option value="most_viewed">阅读量最高</option>
          <option value="title">标题排序</option>
        </select>
      </div>
      
      <div class="search-box">
        <input 
          type="text" 
          v-model="searchQuery" 
          placeholder="搜索文章..." 
          class="search-input"
          @input="debounceSearch"
        />
        <button class="search-btn">
          <span class="search-icon">🔍</span>
        </button>
      </div>
    </div>
    
    <!-- 批量操作按钮 -->
    <div class="batch-actions" v-if="selectedArticles.length > 0">
      <span class="batch-info">已选择 {{ selectedArticles.length }} 篇文章</span>
      <button class="batch-btn" @click="batchPublish">批量发布</button>
      <button class="batch-btn" @click="batchUnpublish">批量下架</button>
      <button class="batch-btn delete" @click="confirmBatchDelete">批量删除</button>
    </div>
    
    <!-- 文章表格 -->
    <div class="table-container">
      <table class="data-table" v-if="filteredArticles.length > 0">
        <thead>
          <tr>
            <th class="th-checkbox">
              <input type="checkbox" v-model="selectAll" @change="toggleSelectAll">
            </th>
            <th class="th-title">标题</th>
            <th class="th-category">分类</th>
            <th class="th-date">发布日期</th>
            <th class="th-status">状态</th>
            <th class="th-views">阅读量</th>
            <th class="th-actions">操作</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="article in filteredArticles" :key="article.id" :class="['table-row', { selected: selectedArticles.includes(article.id) }]">
            <td class="td-checkbox">
              <input type="checkbox" :value="article.id" v-model="selectedArticles">
            </td>
            <td class="td-title">
              <div class="article-title-cell">
                <router-link :to="`/article/${userInfo.username}/${article.slug}`" class="article-title-link">
                  {{ article.title }}
                </router-link>
              </div>
            </td>
            <td class="td-category">
              <div class="category-tags">
                <span v-for="(category, index) in article.categories" :key="index" class="category-tag">
                  {{ category.name }}
                </span>
                <span v-if="!article.categories || article.categories.length === 0" class="no-category">未分类</span>
              </div>
            </td>
            <td class="td-date">{{ formatDate(article.created_at) }}</td>
            <td class="td-status">
              <span :class="['status-badge', article.is_published ? 'published' : 'draft']">
                {{ article.is_published ? '已发布' : '草稿' }}
              </span>
            </td>
            <td class="td-views">{{ article.view_count || 0 }}</td>
            <td class="td-actions">
              <div class="action-buttons">
                <router-link :to="`/edit/${article.id}`" class="action-btn edit" title="编辑">
                  ✏️
                </router-link>
                <button @click="togglePublishStatus(article)" class="action-btn" :title="article.is_published ? '设为草稿' : '发布'">
                  {{ article.is_published ? '📥' : '📤' }}
                </button>
                <button @click="confirmDeleteArticle(article)" class="action-btn delete" title="删除">
                  🗑️
                </button>
              </div>
            </td>
          </tr>
        </tbody>
      </table>
      <div v-else class="empty-state">
        <div class="empty-icon">📝</div>
        <h3>暂无文章</h3>
        <p>您还没有创建任何文章，或没有符合筛选条件的文章</p>
        <router-link to="/create" class="create-article-btn">写一篇新文章</router-link>
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
  </div>
</template>

<script>
export default {
  name: 'ArticlesView',
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
      filters: {
        status: 'all',
        sortBy: 'newest'
      },
      searchQuery: '',
      currentPage: 1,
      itemsPerPage: 10,
      selectedArticles: [],
      selectAll: false,
      debounceTimeout: null
    };
  },
  computed: {
    filteredArticles() {
      let results = [...this.articles];
      
      // 状态过滤
      if (this.filters.status !== 'all') {
        const isPublished = this.filters.status === 'published';
        results = results.filter(article => article.is_published === isPublished);
      }
      
      // 搜索过滤
      if (this.searchQuery.trim()) {
        const query = this.searchQuery.toLowerCase();
        results = results.filter(article => 
          article.title.toLowerCase().includes(query) || 
          (article.summary && article.summary.toLowerCase().includes(query))
        );
      }
      
      // 排序
      switch(this.filters.sortBy) {
        case 'newest':
          results.sort((a, b) => new Date(b.created_at) - new Date(a.created_at));
          break;
        case 'oldest':
          results.sort((a, b) => new Date(a.created_at) - new Date(b.created_at));
          break;
        case 'most_viewed':
          results.sort((a, b) => (b.view_count || 0) - (a.view_count || 0));
          break;
        case 'title':
          results.sort((a, b) => a.title.localeCompare(b.title));
          break;
      }
      
      // 分页
      const startIdx = (this.currentPage - 1) * this.itemsPerPage;
      const endIdx = startIdx + this.itemsPerPage;
      
      return results.slice(startIdx, endIdx);
    },
    
    totalItems() {
      let results = [...this.articles];
      
      // 状态过滤
      if (this.filters.status !== 'all') {
        const isPublished = this.filters.status === 'published';
        results = results.filter(article => article.is_published === isPublished);
      }
      
      // 搜索过滤
      if (this.searchQuery.trim()) {
        const query = this.searchQuery.toLowerCase();
        results = results.filter(article => 
          article.title.toLowerCase().includes(query) || 
          (article.summary && article.summary.toLowerCase().includes(query))
        );
      }
      
      return results.length;
    },
    
    totalPages() {
      return Math.ceil(this.totalItems / this.itemsPerPage);
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
  watch: {
    'filters.status'() {
      this.currentPage = 1;
    },
    'filters.sortBy'() {
      this.currentPage = 1;
    },
    searchQuery() {
      this.currentPage = 1;
    }
  },
  methods: {
    formatDate(dateString) {
      if (!dateString) return '';
      const date = new Date(dateString);
      return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
    },
    
    debounceSearch() {
      clearTimeout(this.debounceTimeout);
      this.debounceTimeout = setTimeout(() => {
        this.currentPage = 1;
      }, 300);
    },
    
    changePage(page) {
      if (page >= 1 && page <= this.totalPages) {
        this.currentPage = page;
        window.scrollTo(0, 0);
      }
    },
    
    toggleSelectAll() {
      if (this.selectAll) {
        this.selectedArticles = this.filteredArticles.map(article => article.id);
      } else {
        this.selectedArticles = [];
      }
    },
    
    onFilterChange() {
      this.currentPage = 1;
      this.selectedArticles = [];
      this.selectAll = false;
    },
    
    confirmDeleteArticle(article) {
      this.$emit('confirm-delete', article);
    },
    
    confirmBatchDelete() {
      this.$emit('confirm-batch-delete', this.selectedArticles);
    },
    
    togglePublishStatus(article) {
      this.$emit('toggle-publish', article);
    },
    
    batchPublish() {
      this.$emit('batch-publish', this.selectedArticles);
    },
    
    batchUnpublish() {
      this.$emit('batch-unpublish', this.selectedArticles);
    }
  }
}
</script>

<style scoped>
.articles-view {
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
  background-color: #e9ecef;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
}

.batch-btn:hover {
  background-color: #dee2e6;
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

.th-title, .td-title {
  min-width: 200px;
}

.th-category, .td-category {
  min-width: 150px;
}

.th-date, .td-date {
  width: 120px;
}

.th-status, .td-status {
  width: 80px;
}

.th-views, .td-views {
  width: 80px;
  text-align: right;
}

.th-actions, .td-actions {
  width: 120px;
  text-align: center;
}

.article-title-link {
  color: #333;
  text-decoration: none;
  font-weight: 500;
  transition: color 0.2s;
}

.article-title-link:hover {
  color: #3498db;
}

.category-tags {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}

.category-tag {
  display: inline-block;
  padding: 2px 8px;
  font-size: 0.8rem;
  background-color: #e9ecef;
  border-radius: 4px;
  color: #495057;
}

.no-category {
  font-size: 0.8rem;
  color: #6c757d;
  font-style: italic;
}

.status-badge {
  display: inline-block;
  padding: 3px 8px;
  border-radius: 12px;
  font-size: 0.8rem;
  font-weight: 500;
}

.status-badge.published {
  background-color: #d4edda;
  color: #155724;
}

.status-badge.draft {
  background-color: #e9ecef;
  color: #6c757d;
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
  margin-bottom: 20px;
}

.create-article-btn {
  display: inline-block;
  padding: 8px 16px;
  background-color: #3498db;
  color: white;
  text-decoration: none;
  border-radius: 4px;
  transition: background-color 0.2s;
}

.create-article-btn:hover {
  background-color: #2980b9;
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

@media (max-width: 992px) {
  .th-category, .td-category,
  .th-views, .td-views {
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
  
  .th-date, .td-date {
    display: none;
  }
  
  .action-buttons {
    flex-direction: column;
    gap: 5px;
  }
}
</style> 