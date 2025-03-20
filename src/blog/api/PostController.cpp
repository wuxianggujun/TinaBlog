#include "PostController.hpp"
#include "../NgxLog.hpp"
#include <stdexcept>

// 获取单例实例
PostController& PostController::getInstance() {
    static PostController instance;
    return instance;
}

// 注册路由
void PostController::registerRoutes(BlogRoute& router) {
    // 获取文章列表
    router.register_route("/api/posts", HttpMethod::GET_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleGetPosts(req, params);
        });
    
    // 获取单篇文章
    router.register_route("/api/posts/:id", HttpMethod::GET_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleGetPost(req, params);
        });
    
    // 创建文章
    router.register_route("/api/posts", HttpMethod::POST_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleCreatePost(req, params);
        }, JwtVerifyLevel::JWT_REQUIRED);
    
    // 更新文章
    router.register_route("/api/posts/:id", HttpMethod::PUT_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleUpdatePost(req, params);
        }, JwtVerifyLevel::JWT_REQUIRED);
    
    // 删除文章
    router.register_route("/api/posts/:id", HttpMethod::DELETE_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleDeletePost(req, params);
        }, JwtVerifyLevel::JWT_REQUIRED);
    
    // 获取分类文章
    router.register_route("/api/categories/:category/posts", HttpMethod::GET_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleGetPostsByCategory(req, params);
        });
    
    // 获取标签文章
    router.register_route("/api/tags/:tag/posts", HttpMethod::GET_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleGetPostsByTag(req, params);
        });
    
    // 获取博客统计信息
    router.register_route("/api/stats", HttpMethod::GET_METHOD, 
        [this](NgxRequest& req, const NgxRouteParams& params) -> ngx_int_t {
            return this->handleGetStats(req, params);
        });
}

// 获取文章列表
ngx_int_t PostController::handleGetPosts(NgxRequest& req, const NgxRouteParams& params) {
    try {
        // 获取分页参数
        auto page_param = req.get_arg("page");
        auto limit_param = req.get_arg("limit");
        
        int page = page_param ? std::stoi(*page_param) : 1;
        int limit = limit_param ? std::stoi(*limit_param) : 10;
        
        if (page < 1) page = 1;
        if (limit < 1 || limit > 50) limit = 10;
        
        int offset = (page - 1) * limit;
        
        // 获取文章列表
        auto posts = BlogPostDao().getAllPosts(limit, offset);
        
        // 构建响应
        json response = json::array();
        for (const auto& post : posts) {
            response.push_back({
                {"id", post.id},
                {"title", post.title},
                {"slug", post.slug},
                {"summary", post.summary},
                {"author", post.author},
                {"created_at", post.created_at},
                {"updated_at", post.updated_at},
                {"categories", post.categories},
                {"tags", post.tags},
                {"view_count", post.view_count}
            });
        }
        
        json result = {
            {"posts", response},
            {"page", page},
            {"limit", limit},
            {"total", BlogPostDao().getPublishedPostCount()}
        };
        
        return req.send_json(result.dump());
    }
    catch (const std::exception& e) {
        req.get_log().error("Error fetching posts: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

// 获取单篇文章
ngx_int_t PostController::handleGetPost(NgxRequest& req, const NgxRouteParams& params) {
    try {
        // 获取文章ID参数
        const ngx_str_t* id_param = params.get("id");
        if (!id_param) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing post ID");
        }
        
        std::string id_str(reinterpret_cast<char*>(id_param->data), id_param->len);
        
        // 检查是否是数字ID或slug
        bool is_numeric = true;
        for (char c : id_str) {
            if (!std::isdigit(c)) {
                is_numeric = false;
                break;
            }
        }
        
        std::optional<BlogPostRecord> post;
        
        // 根据ID或slug获取文章
        if (is_numeric) {
            int id = std::stoi(id_str);
            post = BlogPostDao().getPostById(id);
        } else {
            post = BlogPostDao().getPostBySlug(id_str);
        }
        
        if (!post) {
            return req.send_error(NGX_HTTP_NOT_FOUND, "Post not found");
        }
        
        // 增加浏览次数
        BlogPostDao().incrementViewCount(post->id);
        
        // 构建响应
        json response = {
            {"id", post->id},
            {"title", post->title},
            {"slug", post->slug},
            {"content", post->content},
            {"summary", post->summary},
            {"author", post->author},
            {"created_at", post->created_at},
            {"updated_at", post->updated_at},
            {"categories", post->categories},
            {"tags", post->tags},
            {"view_count", post->view_count + 1} // 即时更新，加1
        };
        
        return req.send_json(response.dump());
    }
    catch (const std::exception& e) {
        req.get_log().error("Error fetching post: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

// 创建文章
ngx_int_t PostController::handleCreatePost(NgxRequest& req, const NgxRouteParams& params) {
    std::string body = req.read_body();
    
    if (body.empty()) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, "Empty request body");
    }
    
    try {
        // 获取用户信息（从JWT）
        auto auth_header = req.get_header("Authorization");
        if (!auth_header) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Missing authorization header");
        }
        
        std::string token = *auth_header;
        // 移除"Bearer "前缀
        if (token.substr(0, 7) == "Bearer ") {
            token = token.substr(7);
        }
        
        // 获取用户名
        auto username_opt = JwtService::getInstance().getUsername(token);
        if (!username_opt) {
            return req.send_error(NGX_HTTP_UNAUTHORIZED, "Invalid token");
        }
        
        std::string username = *username_opt;
        
        // 解析请求体
        json requestData = json::parse(body);
        
        // 验证请求参数
        if (!requestData.contains("title") || 
            !requestData.contains("content")) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing required fields");
        }
        
        std::string title = requestData["title"];
        std::string content = requestData["content"];
        std::string summary = requestData.contains("summary") ? 
                            requestData["summary"].get<std::string>() : "";
        
        // 获取分类和标签
        std::vector<std::string> categories;
        if (requestData.contains("categories") && requestData["categories"].is_array()) {
            for (const auto& cat : requestData["categories"]) {
                categories.push_back(cat.get<std::string>());
            }
        }
        
        std::vector<std::string> tags;
        if (requestData.contains("tags") && requestData["tags"].is_array()) {
            for (const auto& tag : requestData["tags"]) {
                tags.push_back(tag.get<std::string>());
            }
        }
        
        // 发布状态
        bool published = true;
        if (requestData.contains("published")) {
            published = requestData["published"].get<bool>();
        }
        
        // 创建文章
        int post_id = BlogPostDao().createPost(
            title, content, summary, username, categories, tags, published
        );
        
        if (post_id <= 0) {
            return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Failed to create post");
        }
        
        // 返回成功响应
        json response = {
            {"id", post_id},
            {"title", title},
            {"message", "Post created successfully"}
        };
        
        return req.send_json(response.dump());
    }
    catch (const json::exception& e) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON: ") + e.what());
    }
    catch (const std::exception& e) {
        req.get_log().error("Error creating post: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

// 更新文章
ngx_int_t PostController::handleUpdatePost(NgxRequest& req, const NgxRouteParams& params) {
    std::string body = req.read_body();
    
    if (body.empty()) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, "Empty request body");
    }
    
    try {
        // 获取文章ID参数
        const ngx_str_t* id_param = params.get("id");
        if (!id_param) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing post ID");
        }
        
        int post_id = std::stoi(std::string(reinterpret_cast<char*>(id_param->data), id_param->len));
        
        // 解析请求体
        json requestData = json::parse(body);
        
        // 验证请求参数
        if (!requestData.contains("title") || 
            !requestData.contains("content")) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing required fields");
        }
        
        std::string title = requestData["title"];
        std::string content = requestData["content"];
        std::string summary = requestData.contains("summary") ? 
                            requestData["summary"].get<std::string>() : "";
        
        // 发布状态
        bool published = true;
        if (requestData.contains("published")) {
            published = requestData["published"].get<bool>();
        }
        
        // 更新文章
        bool success = BlogPostDao().updatePost(
            post_id, title, content, summary, published
        );
        
        if (!success) {
            return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Failed to update post");
        }
        
        // 获取分类和标签
        if (requestData.contains("categories") && requestData["categories"].is_array()) {
            std::vector<std::string> categories;
            for (const auto& cat : requestData["categories"]) {
                categories.push_back(cat.get<std::string>());
            }
            BlogPostDao().setPostCategories(post_id, categories);
        }
        
        if (requestData.contains("tags") && requestData["tags"].is_array()) {
            std::vector<std::string> tags;
            for (const auto& tag : requestData["tags"]) {
                tags.push_back(tag.get<std::string>());
            }
            BlogPostDao().setPostTags(post_id, tags);
        }
        
        // 返回成功响应
        json response = {
            {"id", post_id},
            {"message", "Post updated successfully"}
        };
        
        return req.send_json(response.dump());
    }
    catch (const json::exception& e) {
        return req.send_error(NGX_HTTP_BAD_REQUEST, std::string("Invalid JSON: ") + e.what());
    }
    catch (const std::exception& e) {
        req.get_log().error("Error updating post: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

// 删除文章
ngx_int_t PostController::handleDeletePost(NgxRequest& req, const NgxRouteParams& params) {
    try {
        // 获取文章ID参数
        const ngx_str_t* id_param = params.get("id");
        if (!id_param) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing post ID");
        }
        
        int post_id = std::stoi(std::string(reinterpret_cast<char*>(id_param->data), id_param->len));
        
        // 删除文章
        bool success = BlogPostDao().deletePost(post_id);
        
        if (!success) {
            return req.send_error(NGX_HTTP_NOT_FOUND, "Post not found or could not be deleted");
        }
        
        // 返回成功响应
        json response = {
            {"id", post_id},
            {"message", "Post deleted successfully"}
        };
        
        return req.send_json(response.dump());
    }
    catch (const std::exception& e) {
        req.get_log().error("Error deleting post: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

// 获取按分类筛选的文章
ngx_int_t PostController::handleGetPostsByCategory(NgxRequest& req, const NgxRouteParams& params) {
    try {
        // 获取分类参数
        const ngx_str_t* category_param = params.get("category");
        if (!category_param) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing category parameter");
        }
        
        std::string category(reinterpret_cast<char*>(category_param->data), category_param->len);
        
        // 获取分页参数
        auto page_param = req.get_arg("page");
        auto limit_param = req.get_arg("limit");
        
        int page = page_param ? std::stoi(*page_param) : 1;
        int limit = limit_param ? std::stoi(*limit_param) : 10;
        
        if (page < 1) page = 1;
        if (limit < 1 || limit > 50) limit = 10;
        
        int offset = (page - 1) * limit;
        
        // 获取文章列表
        auto posts = BlogPostDao().getPostsByCategory(category, limit, offset);
        
        // 构建响应
        json response = json::array();
        for (const auto& post : posts) {
            response.push_back({
                {"id", post.id},
                {"title", post.title},
                {"slug", post.slug},
                {"summary", post.summary},
                {"author", post.author},
                {"created_at", post.created_at},
                {"updated_at", post.updated_at},
                {"categories", post.categories},
                {"tags", post.tags},
                {"view_count", post.view_count}
            });
        }
        
        json result = {
            {"category", category},
            {"posts", response},
            {"page", page},
            {"limit", limit},
            {"total", posts.size()}  // 这里简化处理，实际应当获取该分类的总文章数
        };
        
        return req.send_json(result.dump());
    }
    catch (const std::exception& e) {
        req.get_log().error("Error fetching posts by category: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

// 获取按标签筛选的文章
ngx_int_t PostController::handleGetPostsByTag(NgxRequest& req, const NgxRouteParams& params) {
    try {
        // 获取标签参数
        const ngx_str_t* tag_param = params.get("tag");
        if (!tag_param) {
            return req.send_error(NGX_HTTP_BAD_REQUEST, "Missing tag parameter");
        }
        
        std::string tag(reinterpret_cast<char*>(tag_param->data), tag_param->len);
        
        // 获取分页参数
        auto page_param = req.get_arg("page");
        auto limit_param = req.get_arg("limit");
        
        int page = page_param ? std::stoi(*page_param) : 1;
        int limit = limit_param ? std::stoi(*limit_param) : 10;
        
        if (page < 1) page = 1;
        if (limit < 1 || limit > 50) limit = 10;
        
        int offset = (page - 1) * limit;
        
        // 获取文章列表
        auto posts = BlogPostDao().getPostsByTag(tag, limit, offset);
        
        // 构建响应
        json response = json::array();
        for (const auto& post : posts) {
            response.push_back({
                {"id", post.id},
                {"title", post.title},
                {"slug", post.slug},
                {"summary", post.summary},
                {"author", post.author},
                {"created_at", post.created_at},
                {"updated_at", post.updated_at},
                {"categories", post.categories},
                {"tags", post.tags},
                {"view_count", post.view_count}
            });
        }
        
        json result = {
            {"tag", tag},
            {"posts", response},
            {"page", page},
            {"limit", limit},
            {"total", posts.size()}  // 这里简化处理，实际应当获取该标签的总文章数
        };
        
        return req.send_json(result.dump());
    }
    catch (const std::exception& e) {
        req.get_log().error("Error fetching posts by tag: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
}

// 获取博客统计信息
ngx_int_t PostController::handleGetStats(NgxRequest& req, const NgxRouteParams& params) {
    try {
        BlogPostDao dao;
        
        // 获取统计信息
        int postCount = dao.getPostCount();
        int publishedPostCount = dao.getPublishedPostCount();
        int totalViewCount = dao.getTotalViewCount();
        int categoryCount = dao.getCategoryCount();
        int tagCount = dao.getTagCount();
        
        // 构建响应
        json response = {
            {"post_count", postCount},
            {"published_post_count", publishedPostCount},
            {"draft_post_count", postCount - publishedPostCount},
            {"total_view_count", totalViewCount},
            {"category_count", categoryCount},
            {"tag_count", tagCount}
        };
        
        return req.send_json(response.dump());
    }
    catch (const std::exception& e) {
        req.get_log().error("Error fetching blog stats: %s", e.what());
        return req.send_error(NGX_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    }
} 