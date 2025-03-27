import { createRouter, createWebHistory } from 'vue-router'
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
  {
    path: '/login',
    name: 'login',
    component: () => import('../views/Login.vue'),
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
      // 重定向到新的URL结构，使用默认author参数
      return { name: 'article-detail', params: { author: 'author', slug } };
    }
  },
  // 404页面
  {
    path: '/404',
    name: 'not-found',
    component: NotFound
  },
  // 捕获所有未匹配的路由，定向到404页面
  {
    path: '/:pathMatch(.*)*',
    redirect: { name: 'not-found' }
  }
]

const router = createRouter({
  history: createWebHistory(),
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
