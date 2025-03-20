#pragma once

#include "../NgxRequest.hpp"
#include "../BlogRoute.hpp"
#include "../db/BlogPostDao.hpp"
#include "../service/JwtService.hpp"
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

/**
 * @brief 博客文章控制器类，处理文章的CRUD操作
 */
class PostController {
public:
    /**
     * @brief 获取PostController单例
     * @return PostController单例引用
     */
    static PostController& getInstance();
    
    /**
     * @brief 注册路由
     * @param router BlogRoute实例引用
     */
    void registerRoutes(BlogRoute& router);
    
private:
    // 单例模式
    PostController() = default;
    ~PostController() = default;
    PostController(const PostController&) = delete;
    PostController& operator=(const PostController&) = delete;
    
    // API处理方法
    
    /**
     * @brief 获取文章列表
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleGetPosts(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 获取单篇文章
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleGetPost(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 创建文章
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleCreatePost(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 更新文章
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleUpdatePost(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 删除文章
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleDeletePost(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 获取按分类筛选的文章
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleGetPostsByCategory(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 获取按标签筛选的文章
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleGetPostsByTag(NgxRequest& req, const NgxRouteParams& params);
    
    /**
     * @brief 获取博客统计信息
     * @param req NgxRequest对象
     * @param params 路由参数
     * @return ngx_int_t Nginx状态码
     */
    ngx_int_t handleGetStats(NgxRequest& req, const NgxRouteParams& params);
}; 