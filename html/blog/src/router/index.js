import { createRouter, createWebHashHistory } from 'vue-router'
import Home from '../views/Home.vue'
import NotFound from '../views/NotFound.vue'

const routes = [
  {
    path: '/',
    name: 'Home',
    component: Home
  },
  {
    path: '/about',
    name: 'about',
    // route level code-splitting
    // this generates a separate chunk (About.[hash].js) for this route
    // which is lazy-loaded when the route is visited.
    component: () => import('../views/AboutView.vue'),
  },
  // 分类页面路由
  {
    path: '/categories',
    name: 'categories',
    component: () => import('../views/CategoryView.vue'),
    meta: { title: '分类 - Tina博客' }
  },
  // 单个分类页面路由
  {
    path: '/category/:slug',
    name: 'category-detail',
    component: () => import('../views/CategoryDetailView.vue'),
    props: true,
    meta: { title: '分类文章 - Tina博客' }
  },
  // 归档页面路由
  {
    path: '/archives',
    name: 'archives',
    component: () => import('../views/ArchiveView.vue'),
    meta: { title: '归档 - Tina博客' }
  },
  // 个人信息页面路由
  {
    path: '/profile',
    name: 'profile',
    component: () => import('../views/ProfileView.vue'),
    meta: { 
      requiresAuth: true,
      title: '个人信息 - Tina博客'
    }
  },
  // 我的文章页面路由
  {
    path: '/profile/articles',
    name: 'my-articles',
    component: () => import('../views/ProfileView.vue'),
    meta: { 
      requiresAuth: true,
      title: '我的文章 - Tina博客'
    },
    props: { activeTab: 'articles' }
  },
  // 修改密码页面路由
  {
    path: '/profile/password',
    name: 'change-password',
    component: () => import('../views/ProfileView.vue'),
    meta: { 
      requiresAuth: true,
      title: '修改密码 - Tina博客'
    },
    props: { activeTab: 'security' }
  },
  {
    path: '/login',
    name: 'Login',
    component: () => import('../views/Login.vue'),
    meta: {
      requiresGuest: true,
      title: '登录 - Tina博客'
    },
    beforeEnter: (to, from, next) => {
      // 如果用户已登录，重定向到首页
      if (localStorage.getItem('isLoggedIn') === 'true') {
        next('/');
      } else {
        // 验证并设置重定向参数，处理hash路由模式下的特殊情况
        if (!to.query.redirect && from.path !== '/' && !from.path.includes('/login')) {
          // 在hash模式下，重定向需要特殊处理，确保能够重定向回原来的URL
          // 注意：使用fullPath确保包含查询参数
          next({
            path: '/login',
            query: { redirect: from.fullPath }
          });
        } else {
          next();
        }
      }
    }
  },
  {
    path: '/create',
    name: 'create',
    component: () => import('../views/CreatePost.vue'),
    meta: { requiresAuth: true },
    props: true
  },
  {
    path: '/article/:author/:slug',
    name: 'article-detail',
    component: () => import('../views/ArticleDetail.vue'),
    props: true
  },
  // 兼容旧的URL格式
  {
    path: '/article/:slug',
    redirect: to => {
      const { slug } = to.params;
      // 当缺少作者名时，将在文章加载后动态更新URL
      return { name: 'article-detail', params: { author: 'anonymous', slug } };
    }
  },
  // 404页面
  {
    path: '/404',
    name: 'not-found',
    component: NotFound
  },
  {
    path: '/tags',
    name: 'tags',
    component: () => import('../views/TagsView.vue'),
    meta: {
      title: '标签 - Tina博客'
    }
  },
  {
    path: '/tag/:slug',
    name: 'tag',
    component: () => import('../views/TagDetailView.vue'),
    props: true,
    meta: {
      title: '标签文章 - Tina博客'
    }
  },
  // 捕获所有未匹配的路由
  {
    path: '/:pathMatch(.*)*',
    component: Home // 默认回到首页
  }
]

const router = createRouter({
  history: createWebHashHistory(), // 使用hash模式
  routes
})

// 全局路由守卫，检查用户登录状态
router.beforeEach((to, from, next) => {
  console.log(`[Router] 导航到: ${to.path}, 来自: ${from.path}`);
  
  // 获取登录状态和token
  const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
  const token = localStorage.getItem('token');
  const username = localStorage.getItem('username');
  
  console.log(`[Router] 登录状态: isLoggedIn=${isLoggedIn}, hasToken=${!!token}, username=${username || '无'}`);
  
  // 对于登录页面的处理
  if (to.path === '/login') {
    // 如果已登录且有token，重定向到首页
    if (isLoggedIn && token) {
      console.log('[Router] 已完全认证用户尝试访问登录页，重定向到首页');
      next('/');
      return;
    }
    
    // 如果已登录但无token，仍然允许访问登录页
    // 这可能是因为token是HttpOnly的，无法在前端访问
    if (isLoggedIn && !token) {
      console.log('[Router] 已登录但无token的用户访问登录页，允许访问');
    }
    
    // 允许继续访问登录页
    next();
    return;
  }
  
  // 对于需要认证的页面
  if (to.matched.some(record => record.meta.requiresAuth)) {
    // 最严格的验证：需要同时有isLoggedIn和token
    const isFullyAuthenticated = isLoggedIn && token;
    
    // 宽松验证：只需要isLoggedIn为true，不检查token
    // 这适用于token可能是HttpOnly的情况
    const isLooselyAuthenticated = isLoggedIn;
    
    // 使用宽松验证
    if (!isLooselyAuthenticated) {
      console.log('[Router] 未登录用户尝试访问受保护页面，重定向到登录页');
      
      next({ 
        path: '/login',
        query: { redirect: to.fullPath }
      });
      return;
    }
    
    // 如果使用宽松验证通过，但没有token，记录一个警告
    if (isLooselyAuthenticated && !token) {
      console.warn('[Router] 用户是宽松验证状态 (isLoggedIn=true, 无token)');
    }
  }
  
  // 放行
  console.log('[Router] 允许导航');
  next();
});

export default router
