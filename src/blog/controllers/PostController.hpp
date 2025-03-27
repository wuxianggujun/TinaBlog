#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include "blog/db/DbManager.hpp"
#include "blog/utils/ErrorCode.hpp"
#include <memory>
#include <vector>
#include <string>

namespace api {
namespace v1 {

/**
 * 文章控制器
 * 处理文章的CRUD操作
 */
class PostController : public drogon::HttpController<PostController>
{
public:
    METHOD_LIST_BEGIN
    // 创建文章
    ADD_METHOD_TO(PostController::createPost, "/api/posts", drogon::Post, "JwtAuthFilter");
    // 获取文章列表
    ADD_METHOD_TO(PostController::getPosts, "/api/posts", drogon::Get);
    // 获取单篇文章
    ADD_METHOD_TO(PostController::getPost, "/api/posts/{id}", drogon::Get);
    // 更新文章
    ADD_METHOD_TO(PostController::updatePost, "/api/posts/{id}", drogon::Put, "JwtAuthFilter");
    // 删除文章
    ADD_METHOD_TO(PostController::deletePost, "/api/posts/{id}", drogon::Delete, "JwtAuthFilter");
    // 获取分类列表
    ADD_METHOD_TO(PostController::getCategories, "/api/categories", drogon::Get);
    // 获取标签列表
    ADD_METHOD_TO(PostController::getTags, "/api/tags", drogon::Get);
    // 获取用户标签
    ADD_METHOD_TO(PostController::getUserTags, "/api/tags/user", drogon::Get, "JwtAuthFilter");
    // 获取文章列表
    ADD_METHOD_TO(PostController::getArticles, "/api/articles", drogon::Get);
    // 获取文章详情
    ADD_METHOD_TO(PostController::getArticle, "/api/articles/{slug}", drogon::Get);
    // 创建文章
    ADD_METHOD_TO(PostController::createArticle, "/api/articles", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    /**
     * 创建文章
     */
    void createPost(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    /**
     * 获取作者文章列表
     */
    void getPosts(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    /**
     * 获取单篇文章
     */
    void getPost(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                int id) const;

    /**
     * 更新文章
     */
    void updatePost(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   int id) const;

    /**
     * 删除文章
     */
    void deletePost(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   int id) const;

    /**
     * 获取所有分类
     */
    void getCategories(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    /**
     * 获取所有标签
     */
    void getTags(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    /**
     * 获取用户相关标签
     */
    void getUserTags(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    /**
     * 获取公开文章列表
     */
    void getArticles(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    
    /**
     * 获取文章详情
     */
    void getArticle(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback, 
                   const std::string& slug) const;
    
    /**
     * 创建文章
     */
    void createArticle(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

private:
    /**
     * 验证文章数据
     */
    bool validatePostData(const Json::Value& data, std::string& error) const;

    /**
     * 处理文章分类（同步版本）
     */
    void handleCategories(int articleId, const Json::Value& categories) const;

    /**
     * 处理文章标签（同步版本）
     */
    void handleTags(int articleId, const Json::Value& tags) const;

    /**
     * 异步处理文章分类
     */
    void handleCategoriesAsync(int articleId, const Json::Value& categories, std::function<void()> finalCallback) const;

    /**
     * 异步处理文章标签
     */
    void handleTagsAsync(int articleId, const Json::Value& tags, std::function<void()> finalCallback) const;

    /**
     * 创建文章（使用事务）
     */
    void createArticleWithTransaction(DbManager& dbManager,
                                    const std::string& userUuid,
                                    const Json::Value& jsonBody,
                                    const std::function<void(const drogon::HttpResponsePtr&)>& callback) const;
};

} // namespace v1
} // namespace api 