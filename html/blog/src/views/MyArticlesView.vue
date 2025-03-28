<template>
  <div class="admin-container">
    <!-- 左侧导航栏 -->
    <div class="sidebar">
      <div class="sidebar-header">
        <h3>文章管理系统</h3>
      </div>
      <nav class="sidebar-nav">
        <div 
          :class="['nav-item', { active: activeMenu === 'dashboard' }]"
          @click="activeMenu = 'dashboard'"
        >
          <span class="nav-icon">📊</span>
          <span class="nav-text">数据概览</span>
        </div>
        <div 
          :class="['nav-item', { active: activeMenu === 'articles' }]"
          @click="activeMenu = 'articles'"
        >
          <span class="nav-icon">📝</span>
          <span class="nav-text">文章管理</span>
        </div>
        <div 
          :class="['nav-item', { active: activeMenu === 'comments' }]"
          @click="activeMenu = 'comments'"
        >
          <span class="nav-icon">💬</span>
          <span class="nav-text">评论管理</span>
        </div>
        <div 
          :class="['nav-item', { active: activeMenu === 'categories' }]"
          @click="activeMenu = 'categories'"
        >
          <span class="nav-icon">🏷️</span>
          <span class="nav-text">分类管理</span>
        </div>
      </nav>
      <div class="sidebar-footer">
        <router-link to="/" class="back-link">
          <span class="back-icon">🏠</span>
          <span class="back-text">返回主页</span>
        </router-link>
      </div>
    </div>
    
    <!-- 主内容区域 -->
    <div class="main-content">
      <!-- 顶部工具栏 -->
      <div class="top-bar">
        <div class="breadcrumb">
          <span>管理控制台</span>
          <span class="separator">/</span>
          <span>{{ getBreadcrumbTitle() }}</span>
        </div>
        <div class="user-actions">
          <router-link to="/create" class="action-btn create-btn">
            <span class="btn-icon">+</span>
            <span class="btn-text">写文章</span>
          </router-link>
        </div>
      </div>
      
      <!-- 内容区域 -->
      <div class="content-area">
        <!-- 数据概览页面 -->
        <div v-if="activeMenu === 'dashboard'" class="dashboard-view">
          <div class="stats-row">
            <div class="stat-card">
              <div class="stat-icon">📚</div>
              <div class="stat-data">
                <div class="stat-value">{{ articleStats.totalCount || 0 }}</div>
                <div class="stat-label">文章总数</div>
              </div>
            </div>
            <div class="stat-card">
              <div class="stat-icon">👁️</div>
              <div class="stat-data">
                <div class="stat-value">{{ formatNumber(articleStats.totalViews || 0) }}</div>
                <div class="stat-label">总阅读量</div>
              </div>
            </div>
            <div class="stat-card">
              <div class="stat-icon">✅</div>
              <div class="stat-data">
                <div class="stat-value">{{ articleStats.publishedCount || 0 }}</div>
                <div class="stat-label">已发布</div>
              </div>
            </div>
            <div class="stat-card">
              <div class="stat-icon">📝</div>
              <div class="stat-data">
                <div class="stat-value">{{ articleStats.draftCount || 0 }}</div>
                <div class="stat-label">草稿</div>
              </div>
            </div>
          </div>
          
          <!-- 阅读量趋势图 -->
          <div class="chart-container">
            <div class="chart-header">
              <h3>文章阅读量趋势</h3>
              <div class="chart-actions">
                <select v-model="chartTimeRange" class="chart-select">
                  <option value="7">最近7天</option>
                  <option value="30">最近30天</option>
                  <option value="90">最近3个月</option>
                </select>
              </div>
            </div>
            <div class="chart-body">
              <canvas ref="viewsChart" width="800" height="300"></canvas>
            </div>
          </div>
          
          <!-- 热门文章 -->
          <div class="popular-posts-container">
            <div class="section-header">
              <h3>热门文章</h3>
            </div>
            <div class="popular-posts-list">
              <div v-for="(article, index) in popularArticles" :key="article.id" class="popular-post-item">
                <div class="rank">{{ index + 1 }}</div>
                <div class="post-info">
                  <div class="post-title">{{ article.title }}</div>
                  <div class="post-meta">
                    <span class="post-date">{{ formatDate(article.created_at) }}</span>
                    <span class="post-views">{{ formatNumber(article.view_count || 0) }} 次阅读</span>
                  </div>
                </div>
                <div class="post-actions">
                  <router-link :to="`/article/${userInfo.username}/${article.slug}`" class="action-link" title="查看">👁️</router-link>
                  <router-link :to="`/edit/${article.id}`" class="action-link" title="编辑">✏️</router-link>
                </div>
              </div>
              <div v-if="popularArticles.length === 0" class="empty-popular-posts">
                暂无热门文章数据
              </div>
            </div>
          </div>
        </div>
        
        <!-- 文章管理页面 -->
        <div v-if="activeMenu === 'articles'" class="articles-view">
          <div class="filter-bar">
            <div class="filter-group">
              <select v-model="filters.status" class="filter-select">
                <option value="all">所有状态</option>
                <option value="published">已发布</option>
                <option value="draft">草稿</option>
              </select>
              
              <select v-model="filters.sortBy" class="filter-select">
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
                    <div class="category-tags" v-if="article.categories && article.categories.length">
                      <span v-for="category in article.categories.slice(0, 2)" :key="category" class="category-tag">
                        {{ category }}
                      </span>
                      <span v-if="article.categories.length > 2" class="more-tag">+{{ article.categories.length - 2 }}</span>
                    </div>
                    <span v-else class="no-category">无分类</span>
                  </td>
                  <td class="td-date">{{ formatDate(article.created_at) }}</td>
                  <td class="td-status">
                    <span class="status-badge" :class="article.is_published ? 'published' : 'draft'">
                      {{ article.is_published ? '已发布' : '草稿' }}
                    </span>
                  </td>
                  <td class="td-views">{{ formatNumber(article.view_count || 0) }}</td>
                  <td class="td-actions">
                    <div class="action-buttons">
                      <router-link :to="`/edit/${article.id}`" class="action-btn edit-btn" title="编辑">
                        ✏️
                      </router-link>
                      <button @click="togglePublishStatus(article)" class="action-btn publish-btn" :title="article.is_published ? '设为草稿' : '发布'">
                        {{ article.is_published ? '📝' : '📢' }}
                      </button>
                      <button @click="confirmDeleteArticle(article)" class="action-btn delete-btn" title="删除">
                        🗑️
                      </button>
                    </div>
                  </td>
                </tr>
              </tbody>
            </table>
            
            <!-- 空状态 -->
            <div v-else class="empty-state">
              <div class="empty-icon">📭</div>
              <div class="empty-text">
                <p v-if="searchQuery">没有找到匹配"{{ searchQuery }}"的文章</p>
                <p v-else-if="filters.status !== 'all'">{{ filters.status === 'published' ? '没有已发布的文章' : '没有草稿' }}</p>
                <p v-else>您还没有创建任何文章</p>
              </div>
              <router-link to="/create" class="btn primary-btn">开始创作</router-link>
            </div>
          </div>
          
          <!-- 批量操作工具栏 -->
          <div class="batch-actions-bar" v-if="selectedArticles.length > 0">
            <div class="selected-count">已选择 {{ selectedArticles.length }} 篇文章</div>
            <div class="batch-buttons">
              <button @click="batchPublish" class="batch-btn publish-btn">批量发布</button>
              <button @click="batchUnpublish" class="batch-btn unpublish-btn">批量下架</button>
              <button @click="confirmBatchDelete" class="batch-btn delete-btn">批量删除</button>
            </div>
          </div>
          
          <!-- 分页 -->
          <div class="pagination-container" v-if="totalPages > 1">
            <button 
              @click="changePage(currentPage - 1)" 
              :disabled="currentPage === 1"
              class="page-btn prev-btn"
            >
              上一页
            </button>
            
            <div class="page-numbers">
              <button 
                v-for="page in paginationRange" 
                :key="page" 
                @click="changePage(page)"
                :class="['page-number', { active: page === currentPage }]"
              >
                {{ page }}
              </button>
            </div>
            
            <button 
              @click="changePage(currentPage + 1)" 
              :disabled="currentPage === totalPages"
              class="page-btn next-btn"
            >
              下一页
            </button>
          </div>
        </div>
        
        <!-- 其他页面留空，可以后续实现 -->
        <div v-if="activeMenu === 'comments'" class="placeholder-view">
          <div class="coming-soon">
            <div class="coming-soon-icon">🚧</div>
            <div class="coming-soon-text">评论管理功能即将上线</div>
          </div>
        </div>
        
        <div v-if="activeMenu === 'categories'" class="placeholder-view">
          <div class="coming-soon">
            <div class="coming-soon-icon">🚧</div>
            <div class="coming-soon-text">分类管理功能即将上线</div>
          </div>
        </div>
      </div>
    </div>
    
    <!-- 删除确认对话框 -->
    <div v-if="showDeleteConfirm" class="modal-overlay">
      <div class="modal-container">
        <div class="modal-header">
          <h3>{{ isBatchDelete ? '批量删除文章' : '删除文章' }}</h3>
          <button class="close-btn" @click="cancelDelete">×</button>
        </div>
        <div class="modal-body">
          <div class="warning-icon">⚠️</div>
          <div class="confirm-message">
            <p v-if="isBatchDelete">
              您确定要删除选中的 <span class="highlight">{{ selectedArticles.length }}</span> 篇文章吗？
            </p>
            <p v-else>
              您确定要删除文章 "<span class="highlight">{{ articleToDelete?.title }}</span>" 吗？
            </p>
            <p class="warning-text">此操作不可撤销，删除后文章将无法恢复。</p>
          </div>
        </div>
        <div class="modal-footer">
          <button @click="cancelDelete" class="btn cancel-btn">取消</button>
          <button 
            @click="isBatchDelete ? executeBatchDelete() : deleteArticle()" 
            class="btn confirm-btn"
            :disabled="isDeleting"
          >
            {{ isDeleting ? '删除中...' : '确认删除' }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import axios from 'axios';
import Chart from 'chart.js/auto';

export default {
  name: 'MyArticlesView',
  data() {
    return {
      activeMenu: 'dashboard', // 默认显示数据概览
      userInfo: {
        username: ''
      },
      articles: [],
      articleStats: {
        totalCount: 0,
        publishedCount: 0,
        draftCount: 0,
        totalViews: 0
      },
      loading: true,
      error: false,
      errorMessage: '',
      
      // 分页
      currentPage: 1,
      pageSize: 10,
      totalArticles: 0,
      totalPages: 0,
      
      // 搜索和过滤
      searchQuery: '',
      filters: {
        status: 'all',
        sortBy: 'newest'
      },
      
      // 图表相关
      chartTimeRange: '30',
      viewsChart: null,
      viewsData: {
        labels: [],
        values: []
      },
      
      // 文章选择
      selectedArticles: [],
      selectAll: false,
      
      // 删除相关
      showDeleteConfirm: false,
      articleToDelete: null,
      isDeleting: false,
      isBatchDelete: false,
      
      // 防抖计时器
      searchTimeout: null
    };
  },
  computed: {
    // 过滤后的文章列表
    filteredArticles() {
      let result = [...this.articles];
      
      // 按状态过滤
      if (this.filters.status !== 'all') {
        const isPublished = this.filters.status === 'published';
        result = result.filter(article => article.is_published === isPublished);
      }
      
      // 按搜索查询过滤
      if (this.searchQuery) {
        const query = this.searchQuery.toLowerCase();
        result = result.filter(article => 
          article.title.toLowerCase().includes(query) ||
          (article.content && article.content.toLowerCase().includes(query))
        );
      }
      
      // 排序
      switch (this.filters.sortBy) {
        case 'newest':
          result.sort((a, b) => new Date(b.created_at) - new Date(a.created_at));
          break;
        case 'oldest':
          result.sort((a, b) => new Date(a.created_at) - new Date(b.created_at));
          break;
        case 'most_viewed':
          result.sort((a, b) => (b.view_count || 0) - (a.view_count || 0));
          break;
        case 'title':
          result.sort((a, b) => a.title.localeCompare(b.title));
          break;
      }
      
      return result;
    },
    
    // 分页范围
    paginationRange() {
      const range = [];
      const maxVisiblePages = 5;
      
      if (this.totalPages <= maxVisiblePages) {
        // 显示所有页码
        for (let i = 1; i <= this.totalPages; i++) {
          range.push(i);
        }
      } else {
        // 显示有限页码
        let start = Math.max(1, this.currentPage - 2);
        let end = Math.min(this.totalPages, start + maxVisiblePages - 1);
        
        // 调整起始页以显示正确数量的页码
        if (end - start + 1 < maxVisiblePages) {
          start = Math.max(1, end - maxVisiblePages + 1);
        }
        
        for (let i = start; i <= end; i++) {
          range.push(i);
        }
      }
      
      return range;
    },
    
    // 热门文章（按阅读量排序的前5篇）
    popularArticles() {
      return [...this.articles]
        .filter(article => article.is_published)
        .sort((a, b) => (b.view_count || 0) - (a.view_count || 0))
        .slice(0, 5);
    }
  },
  watch: {
    // 监听图表时间范围变化
    chartTimeRange() {
      this.fetchViewsData();
    },
    
    // 监听菜单变化
    activeMenu(newVal) {
      if (newVal === 'dashboard') {
        this.$nextTick(() => {
          this.initViewsChart();
          this.fetchViewsData();
        });
      }
    }
  },
  created() {
    // 设置页面标题
    document.title = '文章管理 - Tina博客';
    this.getUserInfo();
    this.fetchArticles();
  },
  mounted() {
    // 初始化图表
    this.$nextTick(() => {
      this.initViewsChart();
      this.fetchViewsData();
    });
  },
  methods: {
    // 获取面包屑导航标题
    getBreadcrumbTitle() {
      switch (this.activeMenu) {
        case 'dashboard': return '数据概览';
        case 'articles': return '文章管理';
        case 'comments': return '评论管理';
        case 'categories': return '分类管理';
        default: return '数据概览';
      }
    },
    
    // 格式化日期
    formatDate(dateString) {
      if (!dateString) return '';
      
      const options = { year: 'numeric', month: 'short', day: 'numeric' };
      return new Date(dateString).toLocaleDateString('zh-CN', options);
    },
    
    // 格式化数字
    formatNumber(num) {
      if (num >= 1000000) {
        return (num / 1000000).toFixed(1) + 'M';
      } else if (num >= 1000) {
        return (num / 1000).toFixed(1) + 'K';
      } else {
        return num.toString();
      }
    },
    
    // 获取用户信息
    getUserInfo() {
      axios.get('/api/auth/info')
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.userInfo = response.data.data;
          }
        })
        .catch(error => {
          console.error('获取用户信息失败:', error);
          
          // 如果是401错误，跳转到登录页
          if (error.response && error.response.status === 401) {
            this.$router.push('/login?redirect=/my-articles');
          }
        });
    },
    
    // 获取文章列表
    fetchArticles() {
      this.loading = true;
      this.error = false;
      
      axios.get('/api/posts', {
        params: {
          page: this.currentPage,
          pageSize: this.pageSize,
          includeStats: true
        }
      })
        .then(response => {
          if (response.data.code === 0 && response.data.data) {
            this.articles = response.data.data.articles || [];
            this.totalArticles = response.data.data.pagination?.total || 0;
            this.totalPages = response.data.data.pagination?.totalPages || 0;
            
            // 从响应中获取统计数据或手动计算
            if (response.data.data.stats) {
              this.articleStats = response.data.data.stats;
            } else {
              this.calculateStats();
            }
            
            // 重置选择状态
            this.selectedArticles = [];
            this.selectAll = false;
          } else {
            this.error = true;
            this.errorMessage = response.data.message || '获取文章列表失败';
          }
        })
        .catch(error => {
          this.error = true;
          this.errorMessage = error.response?.data?.message || '网络错误，请稍后重试';
          console.error('获取文章列表失败:', error);
        })
        .finally(() => {
          this.loading = false;
        });
    },
    
    // 计算统计数据
    calculateStats() {
      const publishedArticles = this.articles.filter(article => article.is_published);
      const draftArticles = this.articles.filter(article => !article.is_published);
      
      const totalViews = this.articles.reduce((sum, article) => sum + (article.view_count || 0), 0);
      
      this.articleStats = {
        totalCount: this.totalArticles,
        publishedCount: publishedArticles.length,
        draftCount: draftArticles.length,
        totalViews: totalViews
      };
    },
    
    // 初始化阅读量图表
    initViewsChart() {
      if (this.viewsChart) {
        this.viewsChart.destroy();
      }
      
      const ctx = this.$refs.viewsChart?.getContext('2d');
      if (!ctx) return;
      
      this.viewsChart = new Chart(ctx, {
        type: 'line',
        data: {
          labels: this.viewsData.labels,
          datasets: [{
            label: '文章阅读量',
            data: this.viewsData.values,
            backgroundColor: 'rgba(99, 102, 241, 0.2)',
            borderColor: 'rgba(99, 102, 241, 1)',
            borderWidth: 2,
            tension: 0.2,
            pointRadius: 4,
            pointBackgroundColor: 'rgba(99, 102, 241, 1)',
            pointBorderColor: '#fff',
            pointBorderWidth: 2
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          scales: {
            y: {
              beginAtZero: true,
              grid: {
                color: 'rgba(0, 0, 0, 0.05)'
              }
            },
            x: {
              grid: {
                display: false
              }
            }
          },
          plugins: {
            legend: {
              display: false
            },
            tooltip: {
              backgroundColor: 'rgba(0, 0, 0, 0.7)',
              padding: 10,
              titleFont: {
                size: 14
              },
              bodyFont: {
                size: 14
              },
              callbacks: {
                label: (context) => {
                  return `阅读量: ${context.parsed.y}`;
                }
              }
            }
          },
          interaction: {
            mode: 'index',
            intersect: false
          }
        }
      });
    },
    
    // 获取阅读量趋势数据
    fetchViewsData() {
      // 模拟数据 - 实际项目中应该从API获取
      const days = parseInt(this.chartTimeRange);
      const labels = [];
      const values = [];
      
      // 生成最近N天的日期标签
      for (let i = days - 1; i >= 0; i--) {
        const date = new Date();
        date.setDate(date.getDate() - i);
        labels.push(date.toLocaleDateString('zh-CN', { month: 'short', day: 'numeric' }));
        
        // 模拟数据 - 随机生成阅读量
        values.push(Math.floor(Math.random() * 100) + 10);
      }
      
      // 如果有真实API，应该使用下面的代码获取实际数据
      // axios.get('/api/stats/views', {
      //   params: { days: this.chartTimeRange }
      // })
      //   .then(response => {
      //     if (response.data.code === 0) {
      //       this.viewsData.labels = response.data.data.labels;
      //       this.viewsData.values = response.data.data.values;
      //       this.updateChart();
      //     }
      //   })
      //   .catch(error => {
      //     console.error('获取阅读量数据失败:', error);
      //   });
      
      this.viewsData.labels = labels;
      this.viewsData.values = values;
      this.updateChart();
    },
    
    // 更新图表数据
    updateChart() {
      if (this.viewsChart) {
        this.viewsChart.data.labels = this.viewsData.labels;
        this.viewsChart.data.datasets[0].data = this.viewsData.values;
        this.viewsChart.update();
      }
    },
    
    // 分页
    changePage(page) {
      if (page < 1 || page > this.totalPages) return;
      
      this.currentPage = page;
      this.fetchArticles();
    },
    
    // 防抖搜索
    debounceSearch() {
      clearTimeout(this.searchTimeout);
      this.searchTimeout = setTimeout(() => {
        // 客户端搜索，filteredArticles计算属性会自动更新
      }, 300);
    },
    
    // 切换文章发布状态
    togglePublishStatus(article) {
      const newStatus = !article.is_published;
      const action = newStatus ? '发布' : '设为草稿';
      
      axios.put(`/api/posts/${article.id}`, {
        is_published: newStatus
      })
        .then(response => {
          if (response.data.code === 0) {
            // 更新本地文章状态
            article.is_published = newStatus;
            
            // 更新统计数据
            this.calculateStats();
          } else {
            alert(`${action}文章失败: ${response.data.message}`);
          }
        })
        .catch(error => {
          alert(`${action}文章失败: ${error.response?.data?.message || '网络错误'}`);
          console.error(`${action}文章失败:`, error);
        });
    },
    
    // 确认删除单篇文章
    confirmDeleteArticle(article) {
      this.articleToDelete = article;
      this.isBatchDelete = false;
      this.showDeleteConfirm = true;
    },
    
    // 删除单篇文章
    deleteArticle() {
      if (!this.articleToDelete) return;
      
      this.isDeleting = true;
      
      axios.delete(`/api/posts/${this.articleToDelete.id}`)
        .then(response => {
          if (response.data.code === 0) {
            // 从列表中移除已删除的文章
            this.articles = this.articles.filter(article => article.id !== this.articleToDelete.id);
            
            // 更新统计数据
            this.calculateStats();
            
            this.cancelDelete();
            
            // 如果当前页已经没有文章且不是第一页，回到上一页
            if (this.filteredArticles.length === 0 && this.currentPage > 1) {
              this.changePage(this.currentPage - 1);
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
    
    // 取消删除
    cancelDelete() {
      this.showDeleteConfirm = false;
      this.articleToDelete = null;
      this.isBatchDelete = false;
    },
    
    // 全选/取消全选
    toggleSelectAll() {
      if (this.selectAll) {
        this.selectedArticles = this.filteredArticles.map(article => article.id);
      } else {
        this.selectedArticles = [];
      }
    },
    
    // 确认批量删除
    confirmBatchDelete() {
      if (this.selectedArticles.length === 0) return;
      
      this.isBatchDelete = true;
      this.showDeleteConfirm = true;
    },
    
    // 执行批量删除
    executeBatchDelete() {
      if (this.selectedArticles.length === 0) return;
      
      this.isDeleting = true;
      
      // 实际项目应该有批量删除API
      // 这里模拟使用串行删除
      const deletePromises = this.selectedArticles.map(id => 
        axios.delete(`/api/posts/${id}`)
      );
      
      Promise.all(deletePromises)
        .then(responses => {
          // 过滤掉已删除的文章
          this.articles = this.articles.filter(article => !this.selectedArticles.includes(article.id));
          
          // 更新统计数据
          this.calculateStats();
          
          // 清空选择
          this.selectedArticles = [];
          this.selectAll = false;
          
          this.cancelDelete();
          
          // 如果当前页已经没有文章且不是第一页，回到上一页
          if (this.filteredArticles.length === 0 && this.currentPage > 1) {
            this.changePage(this.currentPage - 1);
          }
        })
        .catch(error => {
          alert('批量删除操作部分失败，请刷新后重试');
          console.error('批量删除失败:', error);
        })
        .finally(() => {
          this.isDeleting = false;
        });
    },
    
    // 批量发布
    batchPublish() {
      if (this.selectedArticles.length === 0) return;
      
      // 实际项目应该有批量更新API
      // 这里模拟使用串行更新
      const updatePromises = this.selectedArticles.map(id => 
        axios.put(`/api/posts/${id}`, { is_published: true })
      );
      
      Promise.all(updatePromises)
        .then(responses => {
          // 更新本地文章状态
          this.articles.forEach(article => {
            if (this.selectedArticles.includes(article.id)) {
              article.is_published = true;
            }
          });
          
          // 更新统计数据
          this.calculateStats();
          
          // 清空选择
          this.selectedArticles = [];
          this.selectAll = false;
        })
        .catch(error => {
          alert('批量发布操作部分失败，请刷新后重试');
          console.error('批量发布失败:', error);
        });
    },
    
    // 批量下架
    batchUnpublish() {
      if (this.selectedArticles.length === 0) return;
      
      // 实际项目应该有批量更新API
      // 这里模拟使用串行更新
      const updatePromises = this.selectedArticles.map(id => 
        axios.put(`/api/posts/${id}`, { is_published: false })
      );
      
      Promise.all(updatePromises)
        .then(responses => {
          // 更新本地文章状态
          this.articles.forEach(article => {
            if (this.selectedArticles.includes(article.id)) {
              article.is_published = false;
            }
          });
          
          // 更新统计数据
          this.calculateStats();
          
          // 清空选择
          this.selectedArticles = [];
          this.selectAll = false;
        })
        .catch(error => {
          alert('批量下架操作部分失败，请刷新后重试');
          console.error('批量下架失败:', error);
        });
    }
  }
}
</script>

<style scoped>
/* 基础样式 */
*, *::before, *::after {
  box-sizing: border-box;
}

.admin-container {
  display: flex;
  min-height: 100vh;
  background-color: #f8f9fa;
  color: #333;
}

/* 左侧导航栏 */
.sidebar {
  width: 250px;
  background-color: #2c3e50;
  color: white;
  display: flex;
  flex-direction: column;
  box-shadow: 2px 0 5px rgba(0, 0, 0, 0.1);
}

.sidebar-header {
  padding: 1.5rem;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.sidebar-header h3 {
  margin: 0;
  font-size: 1.2rem;
  font-weight: 600;
}

.sidebar-nav {
  flex: 1;
  padding: 1rem 0;
}

.nav-item {
  padding: 0.8rem 1.5rem;
  display: flex;
  align-items: center;
  cursor: pointer;
  transition: background-color 0.2s;
}

.nav-item:hover {
  background-color: rgba(255, 255, 255, 0.1);
}

.nav-item.active {
  background-color: rgba(255, 255, 255, 0.2);
  border-left: 3px solid #6366f1;
}

.nav-icon {
  margin-right: 0.8rem;
  font-size: 1.1rem;
}

.sidebar-footer {
  padding: 1rem 1.5rem;
  border-top: 1px solid rgba(255, 255, 255, 0.1);
}

.back-link {
  display: flex;
  align-items: center;
  color: white;
  text-decoration: none;
  padding: 0.5rem 0;
  transition: opacity 0.2s;
}

.back-link:hover {
  opacity: 0.8;
}

.back-icon {
  margin-right: 0.8rem;
}

/* 主内容区 */
.main-content {
  flex: 1;
  padding: 0;
  display: flex;
  flex-direction: column;
  overflow-x: auto;
}

/* 顶部工具栏 */
.top-bar {
  background-color: white;
  padding: 1rem 2rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
  border-bottom: 1px solid #eaeaea;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
}

.breadcrumb {
  font-size: 1rem;
  color: #666;
}

.separator {
  margin: 0 0.5rem;
  color: #999;
}

.action-btn {
  padding: 0.5rem 1rem;
  background-color: #6366f1;
  color: white;
  border: none;
  border-radius: 4px;
  font-weight: 500;
  cursor: pointer;
  display: flex;
  align-items: center;
  transition: background-color 0.2s;
}

.action-btn:hover {
  background-color: #4f46e5;
}

.btn-icon {
  margin-right: 0.5rem;
}

/* 内容区 */
.content-area {
  flex: 1;
  padding: 2rem;
  overflow-y: auto;
}

/* 数据概览页 */
.dashboard-view {
  display: flex;
  flex-direction: column;
  gap: 2rem;
}

.stats-row {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 1.5rem;
}

.stat-card {
  background-color: white;
  border-radius: 8px;
  padding: 1.5rem;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
  display: flex;
  align-items: center;
  transition: transform 0.2s, box-shadow 0.2s;
}

.stat-card:hover {
  transform: translateY(-5px);
  box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
}

.stat-icon {
  font-size: 2rem;
  margin-right: 1rem;
  color: #6366f1;
}

.stat-data {
  display: flex;
  flex-direction: column;
}

.stat-value {
  font-size: 2rem;
  font-weight: 700;
  color: #2c3e50;
  line-height: 1;
}

.stat-label {
  font-size: 0.9rem;
  color: #666;
  margin-top: 0.5rem;
}

/* 图表容器 */
.chart-container {
  background-color: white;
  border-radius: 8px;
  padding: 1.5rem;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
}

.chart-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 1rem;
}

.chart-header h3 {
  margin: 0;
  font-size: 1.2rem;
  color: #2c3e50;
}

.chart-select {
  padding: 0.5rem;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 0.9rem;
}

.chart-body {
  height: 300px;
  position: relative;
}

/* 热门文章 */
.popular-posts-container {
  background-color: white;
  border-radius: 8px;
  padding: 1.5rem;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
}

.section-header {
  margin-bottom: 1rem;
}

.section-header h3 {
  margin: 0;
  font-size: 1.2rem;
  color: #2c3e50;
}

.popular-posts-list {
  display: flex;
  flex-direction: column;
  gap: 0.8rem;
}

.popular-post-item {
  display: flex;
  align-items: center;
  padding: 0.8rem;
  border-radius: 6px;
  background-color: #f8f9fa;
  transition: background-color 0.2s;
}

.popular-post-item:hover {
  background-color: #edf2f7;
}

.rank {
  width: 30px;
  height: 30px;
  background-color: #6366f1;
  color: white;
  border-radius: 50%;
  display: flex;
  justify-content: center;
  align-items: center;
  font-weight: bold;
  margin-right: 1rem;
}

.post-info {
  flex: 1;
}

.post-title {
  font-weight: 500;
  margin-bottom: 0.25rem;
}

.post-meta {
  font-size: 0.8rem;
  color: #666;
  display: flex;
  gap: 1rem;
}

.post-actions {
  display: flex;
  gap: 0.5rem;
}

.action-link {
  text-decoration: none;
  color: #666;
  font-size: 1.1rem;
  transition: color 0.2s;
}

.action-link:hover {
  color: #6366f1;
}

.empty-popular-posts {
  padding: 2rem;
  text-align: center;
  color: #666;
}

/* 文章管理页 */
.articles-view {
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}

.filter-bar {
  display: flex;
  justify-content: space-between;
  background-color: white;
  padding: 1rem;
  border-radius: 8px;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
}

.filter-group {
  display: flex;
  gap: 0.8rem;
}

.filter-select {
  padding: 0.5rem 1rem;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 0.9rem;
  min-width: 140px;
}

.search-box {
  position: relative;
  width: 300px;
}

.search-input {
  width: 100%;
  padding: 0.5rem 2.5rem 0.5rem 1rem;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 0.9rem;
}

.search-btn {
  position: absolute;
  right: 0.5rem;
  top: 50%;
  transform: translateY(-50%);
  background: none;
  border: none;
  cursor: pointer;
  font-size: 1rem;
  color: #666;
}

/* 数据表格 */
.table-container {
  background-color: white;
  border-radius: 8px;
  overflow: hidden;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
}

.data-table {
  width: 100%;
  border-collapse: collapse;
  text-align: left;
}

.data-table thead {
  background-color: #f1f5f9;
}

.data-table th,
.data-table td {
  padding: 1rem;
  border-bottom: 1px solid #e5e7eb;
}

.data-table tbody tr {
  transition: background-color 0.2s;
}

.data-table tbody tr:hover {
  background-color: #f8fafc;
}

.data-table tr.selected {
  background-color: #eff6ff;
}

.th-checkbox,
.td-checkbox {
  width: 40px;
  text-align: center;
}

.th-title {
  min-width: 300px;
}

.th-category,
.th-date,
.th-status,
.th-views {
  min-width: 120px;
}

.th-actions {
  width: 120px;
  text-align: center;
}

.article-title-cell {
  max-width: 400px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.article-title-link {
  color: #2c3e50;
  text-decoration: none;
  transition: color 0.2s;
}

.article-title-link:hover {
  color: #6366f1;
}

.category-tags {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
}

.category-tag {
  background-color: #edf2f7;
  color: #4a5568;
  font-size: 0.75rem;
  padding: 0.2rem 0.5rem;
  border-radius: 20px;
}

.more-tag {
  font-size: 0.75rem;
  color: #718096;
}

.no-category {
  color: #a0aec0;
  font-size: 0.85rem;
}

.status-badge {
  display: inline-block;
  padding: 0.25rem 0.5rem;
  border-radius: 20px;
  font-size: 0.75rem;
}

.status-badge.published {
  background-color: #dcfce7;
  color: #166534;
}

.status-badge.draft {
  background-color: #f3f4f6;
  color: #4b5563;
}

.action-buttons {
  display: flex;
  justify-content: center;
  gap: 0.5rem;
}

.action-btn {
  background: none;
  border: none;
  font-size: 1rem;
  cursor: pointer;
  padding: 0.25rem;
  transition: transform 0.2s;
}

.action-btn:hover {
  transform: scale(1.2);
}

.edit-btn {
  color: #3b82f6;
}

.publish-btn {
  color: #10b981;
}

.delete-btn {
  color: #ef4444;
}

/* 空状态 */
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 4rem 2rem;
  text-align: center;
}

.empty-icon {
  font-size: 4rem;
  margin-bottom: 1rem;
  color: #cbd5e1;
}

.empty-text {
  color: #64748b;
  margin-bottom: 1.5rem;
}

.empty-text p {
  margin: 0.5rem 0;
  font-size: 1.1rem;
}

.btn {
  padding: 0.75rem 1.5rem;
  border: none;
  border-radius: 4px;
  font-weight: 500;
  cursor: pointer;
  transition: background-color 0.2s;
}

.primary-btn {
  background-color: #6366f1;
  color: white;
}

.primary-btn:hover {
  background-color: #4f46e5;
}

/* 批量操作工具栏 */
.batch-actions-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  background-color: #334155;
  color: white;
  padding: 0.75rem 1.5rem;
  border-radius: 8px;
  margin-top: 1rem;
}

.selected-count {
  font-weight: 500;
}

.batch-buttons {
  display: flex;
  gap: 0.75rem;
}

.batch-btn {
  padding: 0.5rem 1rem;
  border: none;
  border-radius: 4px;
  font-weight: 500;
  cursor: pointer;
  transition: background-color 0.2s;
}

.batch-btn.publish-btn {
  background-color: #10b981;
  color: white;
}

.batch-btn.publish-btn:hover {
  background-color: #059669;
}

.batch-btn.unpublish-btn {
  background-color: #6b7280;
  color: white;
}

.batch-btn.unpublish-btn:hover {
  background-color: #4b5563;
}

.batch-btn.delete-btn {
  background-color: #ef4444;
  color: white;
}

.batch-btn.delete-btn:hover {
  background-color: #dc2626;
}

/* 分页 */
.pagination-container {
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 1.5rem 0;
  gap: 0.5rem;
}

.page-btn {
  padding: 0.5rem 1rem;
  border: 1px solid #d1d5db;
  background-color: white;
  border-radius: 4px;
  cursor: pointer;
  transition: all 0.2s;
}

.page-btn:hover:not(:disabled) {
  background-color: #f3f4f6;
}

.page-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.page-numbers {
  display: flex;
  gap: 0.25rem;
}

.page-number {
  width: 2.2rem;
  height: 2.2rem;
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid #d1d5db;
  border-radius: 4px;
  cursor: pointer;
  transition: all 0.2s;
}

.page-number:hover:not(.active) {
  background-color: #f3f4f6;
}

.page-number.active {
  background-color: #6366f1;
  color: white;
  border-color: #6366f1;
}

/* 删除确认对话框 */
.modal-overlay {
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

.modal-container {
  background: white;
  border-radius: 8px;
  width: 500px;
  max-width: 90%;
  box-shadow: 0 10px 25px rgba(0, 0, 0, 0.1);
  overflow: hidden;
}

.modal-header {
  padding: 1.5rem;
  border-bottom: 1px solid #e5e7eb;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.modal-header h3 {
  margin: 0;
  font-size: 1.25rem;
  color: #2c3e50;
}

.close-btn {
  background: none;
  border: none;
  font-size: 1.5rem;
  cursor: pointer;
  color: #64748b;
}

.modal-body {
  padding: 2rem;
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
}

.warning-icon {
  font-size: 3rem;
  color: #f59e0b;
  margin-bottom: 1rem;
}

.confirm-message p {
  margin: 0.5rem 0;
  font-size: 1.1rem;
  color: #4b5563;
}

.highlight {
  color: #ef4444;
  font-weight: 600;
}

.warning-text {
  color: #ef4444;
  font-size: 0.9rem;
}

.modal-footer {
  padding: 1.5rem;
  border-top: 1px solid #e5e7eb;
  display: flex;
  justify-content: flex-end;
  gap: 1rem;
}

.btn {
  padding: 0.75rem 1.5rem;
  border: none;
  border-radius: 4px;
  font-weight: 500;
  cursor: pointer;
  transition: background-color 0.2s;
}

.cancel-btn {
  background-color: #e5e7eb;
  color: #4b5563;
}

.cancel-btn:hover {
  background-color: #d1d5db;
}

.confirm-btn {
  background-color: #ef4444;
  color: white;
}

.confirm-btn:hover:not(:disabled) {
  background-color: #dc2626;
}

.confirm-btn:disabled {
  opacity: 0.7;
  cursor: not-allowed;
}

/* 占位区域 */
.placeholder-view {
  height: 400px;
  display: flex;
  justify-content: center;
  align-items: center;
}

.coming-soon {
  text-align: center;
}

.coming-soon-icon {
  font-size: 3rem;
  margin-bottom: 1rem;
  color: #d1d5db;
}

.coming-soon-text {
  font-size: 1.2rem;
  color: #6b7280;
}

/* 响应式调整 */
@media (max-width: 1200px) {
  .stats-row {
    grid-template-columns: repeat(2, 1fr);
  }
}

@media (max-width: 992px) {
  .sidebar {
    width: 70px;
  }
  
  .sidebar-header h3,
  .nav-text,
  .back-text {
    display: none;
  }
  
  .nav-item {
    justify-content: center;
    padding: 1rem;
  }
  
  .nav-icon {
    margin-right: 0;
    font-size: 1.4rem;
  }
  
  .back-link {
    justify-content: center;
  }
  
  .back-icon {
    margin-right: 0;
  }
  
  .th-category,
  .td-category,
  .th-views,
  .td-views {
    display: none;
  }
}

@media (max-width: 768px) {
  .main-content {
    overflow-x: hidden;
  }
  
  .top-bar {
    padding: 1rem;
  }
  
  .content-area {
    padding: 1rem;
  }
  
  .stats-row {
    grid-template-columns: 1fr;
  }
  
  .filter-bar {
    flex-direction: column;
    gap: 1rem;
  }
  
  .search-box {
    width: 100%;
  }
  
  .th-date,
  .td-date {
    display: none;
  }
  
  .batch-actions-bar {
    flex-direction: column;
    gap: 1rem;
  }
}
</style> 