//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogRouter.hpp"
#include <BlogConfig.hpp>
#include "BlogModule.hpp"
#include <fstream>
#include <sstream>

// 处理博客首页
static ngx_int_t handleBlogIndex(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 使用NgxLog记录日志
        NgxLog logger(r);
        logger.info("处理博客首页请求");
        
        // 构建模板变量
        std::unordered_map<std::string, std::string> variables;
        variables["title"] = "我的博客";
        variables["description"] = "欢迎访问我的技术博客";
        variables["author"] = "管理员";
        
        // 添加博客基础路径到模板变量
        variables["blog_base_path"] = config.getBasePath();
        
        // 模拟文章列表 - 由于模板使用了 {{#posts}} 语法，我们需要预处理这部分
        std::string postsHtml = R"(
            <article class="blog-post">
                <a href="/blog/post/1" class="post-title">第一篇文章</a>
                <div class="post-meta">
                    <span>作者: 管理员</span> |
                    <span>发布时间: 2023-05-15</span> |
                    <span>分类: 技术</span>
                </div>
                <div class="post-summary">
                    这是第一篇文章的摘要，介绍了TinaBlog的基本功能和使用方法。
                </div>
                <a href="/blog/post/1" class="read-more">阅读全文 &raquo;</a>
            </article>
            <article class="blog-post">
                <a href="/blog/post/2" class="post-title">第二篇文章</a>
                <div class="post-meta">
                    <span>作者: 管理员</span> |
                    <span>发布时间: 2023-05-20</span> |
                    <span>分类: 教程</span>
                </div>
                <div class="post-summary">
                    这是第二篇文章的摘要，详细讲解了如何定制模板和添加新功能。
                </div>
                <a href="/blog/post/2" class="read-more">阅读全文 &raquo;</a>
            </article>
        )";
        
        // 直接替换模板中的特殊标记
        variables["#posts"] = postsHtml;  // 用于替换 {{#posts}} 和 {{/posts}} 之间的内容
        variables["^posts"] = "";         // 用于替换 没有文章时的提示
        
        // 使用模板引擎渲染响应
        logger.info("使用模板渲染首页");
        return BlogModule::serveTemplateWithVariables(r, "blog_index.html", variables);
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理博客首页异常: %s", e.what());
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
}

// 处理博客文章页
static ngx_int_t handleBlogPost(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 使用NgxLog记录日志
        NgxLog logger(r);
        
        // 获取URI字符串
        std::string uri_str = r->uri.data ? std::string((const char*)r->uri.data, r->uri.len) : "";
        logger.info("处理博客文章请求: %s", uri_str.c_str());
        
        // 获取文章ID参数
        auto it = params.find("id");
        if (it == params.end()) {
            logger.error("缺少文章ID参数");
            return NGX_HTTP_BAD_REQUEST;
        }
        
        std::string postId = it->second;
        
        // 检查ID格式 (可选)
        if (postId.empty() || !std::all_of(postId.begin(), postId.end(), ::isalnum)) {
            logger.error("无效的文章ID: %s", postId.c_str());
            return NGX_HTTP_BAD_REQUEST;
        }
        
        // 构建模板变量
        std::unordered_map<std::string, std::string> variables;
        variables["id"] = postId;
        variables["title"] = "博客文章 #" + postId;
        variables["author"] = "管理员";
        variables["publish_date"] = "2023-05-15";
        variables["category"] = "技术";
        variables["content"] = "这是博客文章 #" + postId + " 的内容。\n\n这里可以加载实际的文章内容。";
        variables["view_count"] = "42";
        
        // 添加博客基础路径到模板变量
        variables["blog_base_path"] = config.getBasePath();
        
        // 检查并使用缓存设置
        if (config.isEnableCache()) {
            variables["cache_enabled"] = "true";
            variables["cache_time"] = std::to_string(config.getCacheTime());
        } else {
            variables["cache_enabled"] = "false";
        }
        
        // 使用模板引擎渲染响应
        logger.info("使用模板渲染文章页面");
        return BlogModule::serveTemplateWithVariables(r, "blog_post.html", variables);
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理博客文章异常: %s", e.what());
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
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
    
    // 清除之前的路由
    router.reset();
    
    // 首页路由 - 同时处理根路径和blog路径
    router.get("/", handleBlogIndex);  // 根路径
    router.get("/index.html", handleBlogIndex);
    router.get("/blog", handleBlogIndex);  // 博客主页
    router.get("/blog/", handleBlogIndex);
    router.get("/blog/index", handleBlogIndex);
    router.get("/blog/index.html", handleBlogIndex);
    
    // 博客文章路由
    router.get("/blog/post/:id", handleBlogPost);
    
    // 分类页面
    router.get("/blog/category/:category", handleBlogCategory);
    
    // 可以继续添加更多路由...
    
    // 使用NgxLog的静态方法记录日志
    NgxLog::log_static(ngx_cycle->log, NGX_LOG_INFO, "博客路由初始化完成，共注册 %d 个路由", router.getRouteCount());
    
    // 打印所有已注册的路由，方便调试
    router.dumpRoutes();
}
