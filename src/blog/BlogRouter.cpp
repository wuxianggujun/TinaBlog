//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogRouter.hpp"
#include "BlogModule.hpp"
#include <fstream>
#include <sstream>

// 处理博客首页
static ngx_int_t handleBlogIndex(ngx_http_request_t* r, const RouteParams& params) {
    return BlogModule::serveTemplate(r, "blog_index.html");
}

// 处理博客文章页
static ngx_int_t handleBlogPost(ngx_http_request_t* r, const RouteParams& params) {
    // 获取文章ID
    auto it = params.find("id");
    if (it == params.end()) {
        return NGX_HTTP_BAD_REQUEST;
    }
    
    std::string postId = it->second;
    
    // 这里可以根据ID加载文章内容
    // ...
    
    // 加载文章模板
    return BlogModule::serveTemplate(r, "blog_post.html");
}

// 处理分类页面
static ngx_int_t handleBlogCategory(ngx_http_request_t* r, const RouteParams& params) {
    auto it = params.find("category");
    if (it == params.end()) {
        return NGX_HTTP_BAD_REQUEST;
    }
    
    std::string category = it->second;
    
    // 根据分类加载文章列表
    // ...
    
    return BlogModule::serveTemplate(r, "blog_category.html");
}

// 初始化所有路由
void initBlogRoutes() {
    auto& router = getBlogRouter();
    
    // 首页路由
    router.get("/", handleBlogIndex);
    router.get("/index.html", handleBlogIndex);
    
    // 博客文章路由
    router.get("/blog/post/:id", handleBlogPost);
    
    // 分类页面
    router.get("/blog/category/:category", handleBlogCategory);
    
    // 可以继续添加更多路由...
}
