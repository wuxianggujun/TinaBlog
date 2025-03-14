//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogRouter.hpp"
#include <BlogConfig.hpp>
#include "BlogModule.hpp"
#include "BlogPostManager.hpp"
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
        
        // 从文章管理器获取文章列表
        auto& manager = BlogPostManager::getInstance();
        const auto posts = manager.getAllPosts(10); // 获取前10篇文章
        
        // 检查是否有文章
        if (!posts.empty()) {
            std::stringstream postsHtml;
            
            for (const auto* post : posts) {
                postsHtml << "<article class=\"blog-post\">\n";
                postsHtml << "  <a href=\"/blog/post/" << post->getId() << "\" class=\"post-title\">" 
                          << post->getTitle() << "</a>\n";
                postsHtml << "  <div class=\"post-meta\">\n";
                postsHtml << "    <span>作者: " << post->getAuthor() << "</span> |\n";
                postsHtml << "    <span>发布时间: " << post->getDate() << "</span> |\n";
                postsHtml << "    <span>分类: " << post->getCategory() << "</span>\n";
                postsHtml << "  </div>\n";
                postsHtml << "  <div class=\"post-summary\">\n";
                postsHtml << "    " << post->getSummary() << "\n";
                postsHtml << "  </div>\n";
                postsHtml << "  <a href=\"/blog/post/" << post->getId() 
                          << "\" class=\"read-more\">阅读全文 &raquo;</a>\n";
                postsHtml << "</article>\n";
            }
            
            // 设置有文章的变量
            variables["#posts"] = postsHtml.str();
            variables["^posts"] = "";  // 无内容
        } else {
            // 设置无文章的变量
            variables["#posts"] = "";  // 无内容
            variables["^posts"] = "<div class=\"no-posts\">暂无文章</div>";
            
            // 如果尚未加载文章管理器，添加示例文章数据
            if (!manager.isLoaded()) {
                // 使用模拟文章列表作为示例
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
                
                // 使用示例数据
                variables["#posts"] = postsHtml;
                variables["^posts"] = "";
            }
        }
        
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
        
        // 检查ID格式
        if (postId.empty()) {
            logger.error("无效的文章ID: %s", postId.c_str());
            return NGX_HTTP_BAD_REQUEST;
        }
        
        // 构建模板变量
        std::unordered_map<std::string, std::string> variables;
        
        // 从文章管理器获取文章
        auto& manager = BlogPostManager::getInstance();
        const BlogPost* post = manager.getPostById(postId);
        
        if (post) {
            // 使用实际文章数据
            variables["id"] = post->getId();
            variables["title"] = post->getTitle();
            variables["author"] = post->getAuthor();
            variables["publish_date"] = post->getDate();
            variables["category"] = post->getCategory();
            variables["content"] = post->toHtml();  // 将Markdown转为HTML
            variables["view_count"] = ""; // 可以添加浏览量统计
            
            // 添加标签
            std::stringstream tagsHtml;
            const auto& tags = post->getTags();
            for (size_t i = 0; i < tags.size(); ++i) {
                tagsHtml << "<a href=\"/blog/tag/" << tags[i] << "\" class=\"tag\">" 
                         << tags[i] << "</a>";
                if (i < tags.size() - 1) {
                    tagsHtml << ", ";
                }
            }
            variables["tags"] = tagsHtml.str();
        } else {
            // 文章不存在或文章管理器尚未初始化，使用示例数据
            logger.warn("文章不存在或未初始化，使用示例数据: %s", postId.c_str());
            
            variables["id"] = postId;
            variables["title"] = "博客文章 #" + postId;
            variables["author"] = "管理员";
            variables["publish_date"] = "2023-05-15";
            variables["category"] = "技术";
            variables["content"] = "这是博客文章 #" + postId + " 的内容。\n\n这里可以加载实际的文章内容。";
            variables["view_count"] = "42";
            variables["tags"] = "<a href=\"/blog/tag/示例\" class=\"tag\">示例</a>, <a href=\"/blog/tag/测试\" class=\"tag\">测试</a>";
        }
        
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

// 处理博客分类页面
static ngx_int_t handleBlogCategory(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 使用NgxLog记录日志
        NgxLog logger(r);
        
        // 获取分类参数
        auto it = params.find("category");
        if (it == params.end()) {
            logger.error("缺少分类参数");
            return NGX_HTTP_BAD_REQUEST;
        }
        
        std::string category = it->second;
        logger.info("处理分类页面请求: %s", category.c_str());
        
        // 构建模板变量
        std::unordered_map<std::string, std::string> variables;
        variables["title"] = "分类: " + category;
        variables["description"] = "查看" + category + "分类下的所有文章";
        variables["category"] = category;
        
        // 添加博客基础路径到模板变量
        variables["blog_base_path"] = config.getBasePath();
        
        // 从文章管理器获取该分类的文章
        auto& manager = BlogPostManager::getInstance();
        const auto posts = manager.getPostsByCategory(category, 10); // 获取前10篇文章
        
        // 检查是否有文章
        if (!posts.empty()) {
            std::stringstream postsHtml;
            
            for (const auto* post : posts) {
                postsHtml << "<article class=\"blog-post\">\n";
                postsHtml << "  <a href=\"/blog/post/" << post->getId() << "\" class=\"post-title\">" 
                          << post->getTitle() << "</a>\n";
                postsHtml << "  <div class=\"post-meta\">\n";
                postsHtml << "    <span>作者: " << post->getAuthor() << "</span> |\n";
                postsHtml << "    <span>发布时间: " << post->getDate() << "</span>\n";
                postsHtml << "  </div>\n";
                postsHtml << "  <div class=\"post-summary\">\n";
                postsHtml << "    " << post->getSummary() << "\n";
                postsHtml << "  </div>\n";
                postsHtml << "  <a href=\"/blog/post/" << post->getId() 
                          << "\" class=\"read-more\">阅读全文 &raquo;</a>\n";
                postsHtml << "</article>\n";
            }
            
            // 设置有文章的变量
            variables["#posts"] = postsHtml.str();
            variables["^posts"] = "";  // 无内容
        } else {
            // 设置无文章的变量
            variables["#posts"] = "";  // 无内容
            variables["^posts"] = "<div class=\"no-posts\">该分类下暂无文章</div>";
        }
        
        // 使用模板引擎渲染响应
        logger.info("使用模板渲染分类页面");
        return BlogModule::serveTemplateWithVariables(r, "blog_category.html", variables);
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理分类页面异常: %s", e.what());
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
}

// 添加新文章的表单处理
static ngx_int_t handleAddPost(ngx_http_request_t* r, const RouteParams& params) {
    // 只接受POST请求
    if (r->method != NGX_HTTP_POST) {
        // 显示添加文章的表单页面
        std::unordered_map<std::string, std::string> variables;
        variables["title"] = "添加新文章";
        variables["error"] = "";
        
        return BlogModule::serveTemplateWithVariables(r, "blog_add_post.html", variables);
    }
    
    try {
        // 封装请求
        NgxRequest request(r);
        
        // 创建日志对象
        NgxLog logger(r);
        logger.info("处理添加新文章请求");
        
        // 读取POST请求体
        // 注意：这里需要实现请求体读取，简化起见仅给出概念实现
        // 实际应用中需要处理请求体的读取和解析
        
        // 模拟获取表单数据
        std::string title = "新文章标题";
        std::string author = "管理员";
        std::string category = "未分类";
        std::string content = "这是新文章的内容。";
        
        // 创建新文章
        auto& manager = BlogPostManager::getInstance();
        std::string postId = manager.createPost(title, author, category, content);
        
        if (!postId.empty()) {
            logger.info("成功创建新文章: %s", postId.c_str());
            
            // 重定向到新文章页面
            ngx_table_elt_t* location = static_cast<ngx_table_elt_t*>(
                ngx_list_push(&r->headers_out.headers));
                
            if (location == nullptr) {
                logger.error("无法创建重定向响应头");
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            
            // 设置Location头
            location->hash = 1;
            location->key.len = sizeof("Location") - 1;
            location->key.data = (u_char*)"Location";
            
            // 设置重定向目标
            std::string redirectUrl = "/blog/post/" + postId;
            location->value.len = redirectUrl.length();
            location->value.data = static_cast<u_char*>(ngx_palloc(r->pool, redirectUrl.length()));
            ngx_memcpy(location->value.data, redirectUrl.c_str(), redirectUrl.length());
            
            // 设置303状态码（See Other）
            r->headers_out.status = NGX_HTTP_SEE_OTHER;
            
            // 发送响应头
            r->headers_out.location = location;
            
            // 设置内容长度为0
            r->headers_out.content_length_n = 0;
            
            // 发送响应头并检查结果
            ngx_int_t rc = ngx_http_send_header(r);
            if (rc == NGX_ERROR || rc > NGX_OK) {
                return rc;
            }
            
            // 创建一个空的响应体
            ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_pcalloc(r->pool, sizeof(ngx_buf_t)));
            if (b == nullptr) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            
            // 正确设置缓冲区
            b->last_buf = 1;
            b->last_in_chain = 1;
            b->pos = b->last = nullptr;
            b->sync = 1;
            
            ngx_chain_t out;
            out.buf = b;
            out.next = nullptr;
            
            return ngx_http_output_filter(r, &out);
        } else {
            logger.error("创建文章失败");
            
            // 返回错误页面
            std::unordered_map<std::string, std::string> variables;
            variables["title"] = "添加新文章";
            variables["error"] = "创建文章失败，请稍后重试";
            
            return BlogModule::serveTemplateWithVariables(r, "blog_add_post.html", variables);
        }
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理添加文章请求异常: %s", e.what());
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
}

// 处理博客首页重定向
static ngx_int_t handleBlogRedirect(ngx_http_request_t* r, const RouteParams& params) {
    NgxLog logger(r);
    logger.info("从/blog/重定向到/");
    
    // 创建重定向响应头
    ngx_table_elt_t* location = static_cast<ngx_table_elt_t*>(
        ngx_list_push(&r->headers_out.headers));
    
    if (location == nullptr) {
        logger.error("无法创建重定向响应头");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 设置Location头
    location->hash = 1;
    location->key.len = sizeof("Location") - 1;
    location->key.data = (u_char*)"Location";
    
    // 设置重定向目标
    location->value.len = sizeof("/") - 1;
    location->value.data = (u_char*)"/"; 
    
    // 设置301永久重定向状态码
    r->headers_out.status = NGX_HTTP_MOVED_PERMANENTLY;
    
    // 设置响应头中的location属性
    r->headers_out.location = location;
    
    // 设置内容长度为0
    r->headers_out.content_length_n = 0;
    
    // 发送响应头并结束请求
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK) {
        return rc;
    }
    
    // 创建一个空的响应体，确保正确设置
    ngx_buf_t* b = static_cast<ngx_buf_t*>(ngx_pcalloc(r->pool, sizeof(ngx_buf_t)));
    if (b == nullptr) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 标记为最后一个缓冲区，但不包含数据（只包含元数据）
    b->last_buf = 1;      // 标记为最后一个缓冲区
    b->last_in_chain = 1; // 标记为链中最后一个
    b->pos = b->last = nullptr; // 明确指示这是一个零大小的缓冲区
    b->sync = 1;          // 标记为同步操作
    
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;
    
    return ngx_http_output_filter(r, &out);
}

// 初始化所有路由
void initBlogRoutes() {
    auto& router = getBlogRouter();
    
    // 清除之前的路由
    router.reset();
    
    // 首页路由 - 只处理根路径
    router.get("/", handleBlogIndex);  // 根路径
    router.get("/index.html", handleBlogIndex);
    
    // 添加博客路径重定向到根路径
    router.get("/blog", handleBlogRedirect);  // 博客主页重定向
    router.get("/blog/", handleBlogRedirect);
    router.get("/blog/index", handleBlogRedirect);
    router.get("/blog/index.html", handleBlogRedirect);
    
    // 博客文章路由
    router.get("/blog/post/:id", handleBlogPost);
    
    // 分类页面
    router.get("/blog/category/:category", handleBlogCategory);
    
    // 文章管理路由
    router.get("/blog/admin/add", handleAddPost);  // 显示添加文章表单
    router.post("/blog/admin/add", handleAddPost); // 处理添加文章请求
    
    // 可以继续添加更多路由...
    
    // 使用NgxLog的静态方法记录日志
    NgxLog::log_static(ngx_cycle->log, NGX_LOG_INFO, "博客路由初始化完成，共注册 %d 个路由", router.getRouteCount());
    
    // 打印所有已注册的路由，方便调试
    router.dumpRoutes();
}
