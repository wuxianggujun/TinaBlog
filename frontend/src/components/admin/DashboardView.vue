<template>
  <div class="dashboard-view">
    <div class="stats-cards">
      <div class="stat-card">
        <div class="stat-icon articles">
          <span class="icon">📄</span>
        </div>
        <div class="stat-data">
          <div class="stat-value">{{ articleStats.totalCount || 0 }}</div>
          <div class="stat-label">文章总数</div>
        </div>
      </div>
      
      <div class="stat-card">
        <div class="stat-icon views">
          <span class="icon">👁️</span>
        </div>
        <div class="stat-data">
          <div class="stat-value">{{ articleStats.totalViews || 0 }}</div>
          <div class="stat-label">总浏览量</div>
        </div>
      </div>
      
      <div class="stat-card">
        <div class="stat-icon comments">
          <span class="icon">💬</span>
        </div>
        <div class="stat-data">
          <div class="stat-value">{{ comments && Array.isArray(comments) ? comments.length : 0 }}</div>
          <div class="stat-label">评论数</div>
        </div>
      </div>
      
      <div class="stat-card">
        <div class="stat-icon categories">
          <span class="icon">🏷️</span>
        </div>
        <div class="stat-data">
          <div class="stat-value">{{ categories && Array.isArray(categories) ? categories.length : 0 }}</div>
          <div class="stat-label">分类数</div>
        </div>
      </div>
    </div>
    
    <div class="chart-section">
      <div class="section-header">
        <h3>访问统计 <span class="mock-data-tag">(模拟数据)</span></h3>
        <div class="time-filter">
          <button 
            class="time-btn" 
            :class="{ active: currentTimeRange === '7d' }"
            @click="setTimeRange('7d')"
          >
            7天
          </button>
          <button 
            class="time-btn" 
            :class="{ active: currentTimeRange === '30d' }"
            @click="setTimeRange('30d')"
          >
            30天
          </button>
          <button 
            class="time-btn" 
            :class="{ active: currentTimeRange === '90d' }"
            @click="setTimeRange('90d')"
          >
            90天
          </button>
        </div>
      </div>
      
      <div class="static-chart">
        <div class="chart-message">
          <div v-if="!articles || articles.length === 0" class="no-data-message">
            暂无数据可供展示
          </div>
          <div v-else class="data-bars">
            <div 
              v-for="(value, index) in staticChartData" 
              :key="index" 
              class="data-bar"
              :style="{ height: `${(value / maxValue) * 100}%` }"
              :title="`${staticChartLabels[index]}: ${value} 次访问`"
            >
              <div class="bar-label">{{ staticChartLabels[index] }}</div>
            </div>
          </div>
        </div>
      </div>
    </div>
    
    <div class="content-section">
      <div class="section-row">
        <div class="popular-articles">
          <div class="section-header">
            <h3>热门文章</h3>
          </div>
          <div class="article-list" v-if="popularArticles.length">
            <div v-for="(article, index) in popularArticles" :key="article.id" class="popular-article">
              <div class="rank">{{ index + 1 }}</div>
              <div class="article-info">
                <h4 class="article-title">
                  <router-link :to="`/article/${article.id}`">{{ article.title }}</router-link>
                </h4>
                <div class="article-meta">
                  <span class="views-count">{{ article.views }} 阅读</span>
                  <span class="dot-divider">•</span>
                  <span class="publish-date">{{ formatDate(article.created_at) }}</span>
                </div>
              </div>
            </div>
          </div>
          <div v-else class="empty-state">
            <p>暂无数据</p>
          </div>
        </div>
        
        <div class="recent-comments">
          <div class="section-header">
            <h3>最新评论</h3>
          </div>
          <div class="comment-list" v-if="recentComments.length">
            <div v-for="comment in recentComments" :key="comment.id" class="recent-comment">
              <div class="comment-avatar">
                {{ getInitials(comment.author_name) }}
              </div>
              <div class="comment-content">
                <div class="comment-author">{{ comment.author_name }}</div>
                <div class="comment-text">{{ truncateText(comment.content, 60) }}</div>
                <div class="comment-meta">
                  <span class="comment-article">在《{{ comment.article_title }}》下</span>
                  <span class="dot-divider">•</span>
                  <span class="comment-date">{{ formatDate(comment.created_at) }}</span>
                </div>
              </div>
            </div>
          </div>
          <div v-else class="empty-state">
            <p>暂无评论</p>
          </div>
        </div>
      </div>
    </div>
    
    <div class="activity-section">
      <div class="section-header">
        <h3>活动日历</h3>
      </div>
      <div class="calendar-container">
        <div class="calendar-header">
          <button class="month-nav prev" @click="changeMonth(-1)">◄</button>
          <div class="current-month">{{ currentMonth }}</div>
          <button class="month-nav next" @click="changeMonth(1)">►</button>
        </div>
        <div class="calendar-week-days">
          <div v-for="day in weekDays" :key="day" class="week-day">{{ day }}</div>
        </div>
        <div class="calendar-days">
          <div 
            v-for="(day, index) in calendarDays" 
            :key="index"
            class="calendar-day"
            :class="{
              'empty': !day.date,
              'today': day.isToday,
              'has-activity': day.activityLevel > 0
            }"
            :title="day.date ? `${day.activityCount} 活动` : ''"
          >
            <span v-if="day.date" class="day-number">{{ day.date.getDate() }}</span>
            <div v-if="day.activityLevel" class="activity-indicator" :class="`level-${day.activityLevel}`"></div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'DashboardView',
  props: {
    articles: {
      type: Array,
      default: () => []
    },
    comments: {
      type: Array,
      default: () => []
    },
    categories: {
      type: Array,
      default: () => []
    }
  },
  data() {
    return {
      articleStats: {
        totalCount: 0,
        publishedCount: 0,
        draftCount: 0,
        totalViews: 0
      },
      currentTimeRange: '7d',
      activities: [],
      calendarDate: new Date(),
      calendarDays: [],
      // 静态图表数据
      staticChartLabels: [],
      staticChartData: [],
      // 星期标签
      weekDays: ['日', '一', '二', '三', '四', '五', '六']
    };
  },
  computed: {
    maxValue() {
      if (this.staticChartData.length === 0) return 100;
      return Math.max(...this.staticChartData) || 100;
    },
    popularArticles() {
      if (!this.articles || !Array.isArray(this.articles)) return [];
      
      return Array.isArray(this.articles) ? 
        [...this.articles]
          .filter(a => a && typeof a === 'object')
          .sort((a, b) => (b.views || 0) - (a.views || 0))
          .slice(0, 5) : [];
    },
    recentComments() {
      if (!this.comments || !Array.isArray(this.comments)) return [];
      
      return Array.isArray(this.comments) ?
        [...this.comments]
          .filter(c => c && typeof c === 'object' && c.created_at)
          .sort((a, b) => new Date(b.created_at) - new Date(a.created_at))
          .slice(0, 5) : [];
    },
    currentMonth() {
      const months = ['一月', '二月', '三月', '四月', '五月', '六月', '七月', '八月', '九月', '十月', '十一月', '十二月'];
      return `${months[this.calendarDate.getMonth()]} ${this.calendarDate.getFullYear()}`;
    }
  },
  watch: {
    currentTimeRange() {
      this.generateStaticChartData();
    },
    articles: {
      handler() {
        this.processArticleStats();
        this.generateActivities();
        this.generateStaticChartData();
      },
      deep: true
    },
    comments: {
      handler() {
        this.generateActivities();
      },
      deep: true
    }
  },
  mounted() {
    this.processArticleStats();
    this.generateActivities();
    this.updateCalendar();
    this.generateStaticChartData();
  },
  methods: {
    processArticleStats() {
      // 计算文章统计数据
      if (!this.articles) return;
      
      const totalCount = this.articles.length;
      const publishedCount = this.articles.filter(article => article && article.is_published).length;
      const draftCount = this.articles.filter(article => article && !article.is_published).length;
      const totalViews = 0; // 假设后端已经把views设为0
      
      this.articleStats = {
        totalCount,
        publishedCount,
        draftCount,
        totalViews
      };
    },
    
    // 生成静态图表数据
    generateStaticChartData() {
      // 清空旧数据
      this.staticChartLabels = [];
      this.staticChartData = [];
      
      // 获取数据范围
      const days = this.currentTimeRange === '7d' ? 7 : 
                   this.currentTimeRange === '30d' ? 30 : 90;
      
      // 生成日期标签和模拟数据
      const endDate = new Date();
      const startDate = new Date();
      startDate.setDate(endDate.getDate() - days + 1);
      
      // 填充日期标签和随机数据点
      for (let date = new Date(startDate); date <= endDate; date.setDate(date.getDate() + 1)) {
        const dateStr = this.formatDateShort(date);
        this.staticChartLabels.push(dateStr);
        
        // 生成模拟数据点
        const randomViews = Math.floor(Math.random() * 50) + 10;
        this.staticChartData.push(randomViews);
      }
    },
    
    setTimeRange(range) {
      this.currentTimeRange = range;
    },
    
    formatDate(dateStr) {
      if (!dateStr) return '';
      const date = new Date(dateStr);
      return date.toLocaleDateString('zh-CN', { 
        year: 'numeric', 
        month: 'short', 
        day: 'numeric' 
      });
    },
    
    formatDateShort(date) {
      return date.toLocaleDateString('zh-CN', { 
        month: 'short', 
        day: 'numeric' 
      });
    },
    
    truncateText(text, maxLength) {
      if (!text) return '';
      return text.length > maxLength ? text.substring(0, maxLength) + '...' : text;
    },
    
    getInitials(name) {
      if (!name) return '?';
      return name.charAt(0).toUpperCase();
    },
    
    changeMonth(step) {
      const newDate = new Date(this.calendarDate);
      newDate.setMonth(newDate.getMonth() + step);
      this.calendarDate = newDate;
      this.updateCalendar();
    },
    
    updateCalendar() {
      const year = this.calendarDate.getFullYear();
      const month = this.calendarDate.getMonth();
      
      // 获取当月第一天
      const firstDay = new Date(year, month, 1);
      // 获取当月最后一天
      const lastDay = new Date(year, month + 1, 0);
      
      // 获取当月第一天是星期几
      const firstDayIndex = firstDay.getDay();
      // 获取当月天数
      const daysInMonth = lastDay.getDate();
      
      const today = new Date();
      const isCurrentMonth = today.getFullYear() === year && today.getMonth() === month;
      
      const days = [];
      
      // 前面的空白天数
      for (let i = 0; i < firstDayIndex; i++) {
        days.push({ date: null });
      }
      
      // 当月的天数
      for (let i = 1; i <= daysInMonth; i++) {
        const date = new Date(year, month, i);
        const isToday = isCurrentMonth && today.getDate() === i;
        
        // 计算该日的活动数量
        const activities = this.getActivitiesForDate(date);
        const activityCount = activities.length;
        
        // 基于活动数量确定活动级别 (0-3)
        let activityLevel = 0;
        if (activityCount > 0) {
          if (activityCount <= 2) activityLevel = 1;
          else if (activityCount <= 5) activityLevel = 2;
          else activityLevel = 3;
        }
        
        days.push({ 
          date, 
          isToday,
          activityCount, 
          activityLevel
        });
      }
      
      // 根据需要填充剩余的空白格子以达到42个（7×6网格）
      const totalDays = 42;
      const remainingDays = totalDays - days.length;
      for (let i = 0; i < remainingDays; i++) {
        days.push({ date: null });
      }
      
      this.calendarDays = days;
    },
    
    generateActivities() {
      // 清空旧的活动数据
      this.activities = [];
      
      // 确保articles是数组
      if (this.articles && Array.isArray(this.articles)) {
        // 添加文章发布活动
        this.articles.forEach(article => {
          if (article && article.created_at) {
            try {
              const date = new Date(article.created_at);
              if (!isNaN(date.getTime())) { // 验证是有效的日期
                // 创建一个全新的对象，避免引用文章对象
                this.activities.push({
                  type: 'article',
                  date: new Date(date.getTime()), // 创建一个新的日期对象，避免引用
                  title: article.title || '无标题文章',
                  id: article.id,
                  key: `article-${article.id}-${date.getTime()}` // 添加唯一键
                });
              }
            } catch (e) {
              console.error('无效的文章日期', article.created_at);
            }
          }
        });
      }
      
      // 确保comments是数组
      if (this.comments && Array.isArray(this.comments)) {
        // 添加评论活动
        this.comments.forEach(comment => {
          if (comment && comment.created_at) {
            try {
              const date = new Date(comment.created_at);
              if (!isNaN(date.getTime())) { // 验证是有效的日期
                // 创建一个全新的对象，避免引用评论对象
                this.activities.push({
                  type: 'comment',
                  date: new Date(date.getTime()), // 创建一个新的日期对象，避免引用
                  author: comment.author_name || '匿名用户',
                  contentSummary: comment.content ? 
                    (comment.content.length > 20 ? comment.content.substring(0, 20) + '...' : comment.content) : '',
                  id: comment.id,
                  key: `comment-${comment.id}-${date.getTime()}` // 添加唯一键
                });
              }
            } catch (e) {
              console.error('无效的评论日期', comment.created_at);
            }
          }
        });
      }
    },
    
    getActivitiesForDate(date) {
      // 验证this.activities是否为有效数组
      if (!this.activities || !Array.isArray(this.activities)) {
        return [];
      }
      
      // 验证date参数是否为有效日期
      if (!date || !(date instanceof Date) || isNaN(date.getTime())) {
        return [];
      }
      
      // 安全地过滤活动列表
      return this.activities.filter(activity => {
        // 确保activity和activity.date都是有效的
        if (!activity || !activity.date) {
          return false;
        }
        
        // 确保activity.date是有效的Date对象
        const activityDate = activity.date instanceof Date ? 
          activity.date : 
          (typeof activity.date === 'string' ? new Date(activity.date) : null);
          
        if (!activityDate || isNaN(activityDate.getTime())) {
          return false;
        }
        
        // 比较日期的年、月、日
        return activityDate.getFullYear() === date.getFullYear() &&
               activityDate.getMonth() === date.getMonth() &&
               activityDate.getDate() === date.getDate();
      });
    }
  }
}
</script>

<style scoped>
.dashboard-view {
  padding: 0 16px;
}

/* 统计卡片样式 */
.stats-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 16px;
  margin-bottom: 24px;
}

.stat-card {
  background-color: #fff;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  padding: 16px;
  display: flex;
  align-items: center;
}

.stat-icon {
  width: 50px;
  height: 50px;
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-right: 16px;
}

.icon {
  font-size: 1.8rem;
}

.stat-icon.articles {
  background-color: rgba(52, 152, 219, 0.1);
  color: #3498db;
}

.stat-icon.views {
  background-color: rgba(46, 204, 113, 0.1);
  color: #2ecc71;
}

.stat-icon.comments {
  background-color: rgba(155, 89, 182, 0.1);
  color: #9b59b6;
}

.stat-icon.categories {
  background-color: rgba(241, 196, 15, 0.1);
  color: #f1c40f;
}

.stat-data {
  flex: 1;
}

.stat-value {
  font-size: 1.6rem;
  font-weight: 700;
  margin-bottom: 4px;
}

.stat-label {
  font-size: 0.9rem;
  color: #7f8c8d;
}

/* 图表区域样式 */
.chart-section {
  background-color: #fff;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  padding: 16px;
  margin-bottom: 24px;
}

.section-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
}

.section-header h3 {
  margin: 0;
  font-size: 1.1rem;
  font-weight: 600;
}

.time-filter {
  display: flex;
  gap: 8px;
}

.time-btn {
  padding: 6px 12px;
  border: 1px solid #e0e0e0;
  background-color: transparent;
  border-radius: 4px;
  font-size: 0.9rem;
  cursor: pointer;
  transition: all 0.2s;
}

.time-btn.active {
  background-color: #3498db;
  color: white;
  border-color: #3498db;
}

.static-chart {
  height: 300px;
  position: relative;
  margin-top: 20px;
}

.chart-message {
  height: 100%;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
}

.no-data-message {
  color: #7f8c8d;
  font-size: 1.1rem;
  text-align: center;
  padding: 40px 0;
}

.data-bars {
  display: flex;
  justify-content: space-between;
  align-items: flex-end;
  width: 100%;
  height: 250px;
  padding: 0 10px;
}

.data-bar {
  flex: 1;
  max-width: 30px;
  min-width: 8px;
  background-color: #3498db;
  margin: 0 3px;
  border-radius: 4px 4px 0 0;
  position: relative;
  transition: all 0.3s;
}

.data-bar:hover {
  background-color: #2980b9;
  transform: scaleY(1.05);
}

.bar-label {
  position: absolute;
  bottom: -20px;
  left: 50%;
  transform: translateX(-50%);
  white-space: nowrap;
  font-size: 0.7rem;
  color: #7f8c8d;
}

/* 内容区域样式 */
.content-section {
  margin-bottom: 24px;
}

.section-row {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 24px;
}

.popular-articles, .recent-comments {
  background-color: #fff;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  padding: 16px;
}

/* 热门文章样式 */
.article-list {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.popular-article {
  display: flex;
  align-items: center;
  padding: 12px;
  border-radius: 6px;
  transition: background-color 0.2s;
}

.popular-article:hover {
  background-color: #f8f9fa;
}

.rank {
  width: 30px;
  height: 30px;
  display: flex;
  align-items: center;
  justify-content: center;
  background-color: #f5f5f5;
  border-radius: 50%;
  font-weight: 700;
  margin-right: 12px;
}

.article-info {
  flex: 1;
}

.article-title {
  margin: 0 0 4px;
  font-size: 1rem;
}

.article-title a {
  color: #333;
  text-decoration: none;
  transition: color 0.2s;
}

.article-title a:hover {
  color: #3498db;
}

.article-meta {
  font-size: 0.85rem;
  color: #7f8c8d;
}

.dot-divider {
  margin: 0 5px;
}

/* 最新评论样式 */
.comment-list {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.recent-comment {
  display: flex;
  padding: 12px;
  border-radius: 6px;
  transition: background-color 0.2s;
}

.recent-comment:hover {
  background-color: #f8f9fa;
}

.comment-avatar {
  width: 36px;
  height: 36px;
  background-color: #3498db;
  color: white;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 600;
  margin-right: 12px;
  flex-shrink: 0;
}

.comment-content {
  flex: 1;
}

.comment-author {
  font-weight: 600;
  margin-bottom: 4px;
}

.comment-text {
  font-size: 0.95rem;
  color: #333;
  margin-bottom: 6px;
  line-height: 1.4;
}

.comment-meta {
  font-size: 0.85rem;
  color: #7f8c8d;
}

.empty-state {
  text-align: center;
  padding: 20px 0;
  color: #7f8c8d;
}

/* 活动日历样式 */
.activity-section {
  background-color: #fff;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  padding: 16px;
}

.calendar-container {
  margin-top: 16px;
}

.calendar-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
}

.month-nav {
  background: none;
  border: 1px solid #e0e0e0;
  border-radius: 4px;
  padding: 6px 12px;
  cursor: pointer;
  transition: all 0.2s;
}

.month-nav:hover {
  background-color: #f5f5f5;
}

.current-month {
  font-weight: 600;
  font-size: 1.1rem;
}

.calendar-week-days {
  display: grid;
  grid-template-columns: repeat(7, 1fr);
  gap: 8px;
  text-align: center;
  margin-bottom: 8px;
}

.week-day {
  font-weight: 600;
  color: #7f8c8d;
  font-size: 0.9rem;
}

.calendar-days {
  display: grid;
  grid-template-columns: repeat(7, 1fr);
  gap: 8px;
}

.calendar-day {
  height: 36px;
  display: flex;
  align-items: center;
  justify-content: center;
  position: relative;
  border-radius: 4px;
  cursor: default;
}

.calendar-day:not(.empty):hover {
  background-color: #f5f5f5;
}

.calendar-day.today {
  background-color: #f5f5f5;
  font-weight: 600;
}

.day-number {
  position: relative;
  z-index: 1;
}

.activity-indicator {
  position: absolute;
  bottom: 4px;
  width: 100%;
  height: 3px;
  border-radius: 2px;
}

.activity-indicator.level-1 {
  width: 50%;
  background-color: #3498db;
  opacity: 0.3;
}

.activity-indicator.level-2 {
  width: 60%;
  background-color: #3498db;
  opacity: 0.6;
}

.activity-indicator.level-3 {
  width: 70%;
  background-color: #3498db;
  opacity: 0.9;
}

/* 响应式调整 */
@media (max-width: 768px) {
  .stats-cards {
    grid-template-columns: repeat(2, 1fr);
  }
  
  .section-row {
    grid-template-columns: 1fr;
  }
  
  .time-filter {
    flex-wrap: wrap;
  }
  
  .data-bars {
    height: 200px;
  }
  
  .data-bar {
    margin: 0 1px;
    min-width: 5px;
  }
  
  .bar-label {
    font-size: 0.6rem;
    transform: translateX(-50%) rotate(-45deg);
    transform-origin: top left;
  }
}

.mock-data-tag {
  font-size: 0.7rem;
  color: #e74c3c;
  font-weight: normal;
  margin-left: 8px;
}
</style> 