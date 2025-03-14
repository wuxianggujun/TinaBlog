//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogRouter.hpp"
#include "BlogModule.hpp"
#include <fstream>
#include <sstream>

// 处理博客首页
static ngx_int_t handleBlogIndex(ngx_http_request_t* r, const RouteParams& params) {

    // 构建模板变量
    std::unordered_map<std::string, std::string> variables;
    variables["title"] = "博客文章 #";
    variables["author"] = "管理员";
    variables["publish_date"] = "2023-05-15";
    variables["category"] = "技术";
    variables["content"] = "这是博客主页的介绍。\n\n这是一个示例博客文章，展示了模板引擎的功能。";
    variables["view_count"] = "42";
    
    return BlogModule::serveTemplateWithVariables(r, "blog_index.html", variables);
}

// 处理博客文章页
static ngx_int_t handleBlogPost(ngx_http_request_t* r, const RouteParams& params) {
    // 获取文章ID
    auto it = params.find("id");
    if (it == params.end()) {
        return NGX_HTTP_BAD_REQUEST;
    }
    
    std::string postId = it->second;
    
    // 构建模板变量
    std::unordered_map<std::string, std::string> variables;
    variables["id"] = postId;
    variables["title"] = "博客文章 #" + postId;
    variables["author"] = "管理员";
    variables["publish_date"] = "2023-05-15";
    variables["category"] = "技术";
    variables["content"] = "这是博客文章 #" + postId + " 的内容。";
    variables["view_count"] = "42";
    
    // 加载并渲染模板
    return BlogModule::serveTemplateWithVariables(r, "blog_post.html", variables);
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
