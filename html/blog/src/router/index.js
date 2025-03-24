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
  const isLoggedIn = localStorage.getItem('isLoggedIn') === 'true';
  const token = localStorage.getItem('token');
  
  // 验证用户是否真正登录：既需要isLoggedIn为true，又需要token存在
  const isAuthenticated = isLoggedIn && token;
  
  // 如果页面需要认证且用户未登录，则重定向到登录页
  if (to.matched.some(record => record.meta.requiresAuth) && !isAuthenticated) {
    // 清理可能不一致的登录状态
    if (isLoggedIn && !token) {
      localStorage.removeItem('isLoggedIn');
      localStorage.removeItem('username');
    }
    
    next({ 
      path: '/login',
      query: { redirect: to.fullPath } // 保存尝试访问的页面，登录后可以重定向回来
    });
  } else {
    next();
  }
});

export default router
