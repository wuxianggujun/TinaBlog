import { createRouter, createWebHistory } from 'vue-router'
import Home from '../views/Home.vue'

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
  
  // 验证用户是否真正登录：既需要isLoggedIn为true，又需要token存在
  const isAuthenticated = isLoggedIn && token;
  
  console.log(`[Router] 认证状态: ${isAuthenticated ? '已认证' : '未认证'}`);
  
  // 如果访问登录页但已经登录，重定向到首页
  if (to.path === '/login' && isAuthenticated) {
    console.log('[Router] 已登录用户尝试访问登录页，重定向到首页');
    next('/');
    return;
  }
  
  // 如果页面需要认证且用户未登录，则重定向到登录页
  if (to.matched.some(record => record.meta.requiresAuth) && !isAuthenticated) {
    console.log('[Router] 未授权用户尝试访问受保护页面，重定向到登录页');
    
    // 清理可能不一致的登录状态
    if (isLoggedIn && !token) {
      console.warn('[Router] 清理不一致的登录状态');
      localStorage.removeItem('isLoggedIn');
      localStorage.removeItem('username');
    }
    
    next({ 
      path: '/login',
      query: { redirect: to.fullPath } // 保存尝试访问的页面，登录后可以重定向回来
    });
  } else {
    // 放行
    console.log('[Router] 允许导航');
    next();
  }
});

export default router
