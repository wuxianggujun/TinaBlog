#pragma once
#include <drogon/HttpController.h>
#include "blog/db/DbManager.hpp"
#include "blog/utils/ErrorCode.hpp"

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
    METHOD_LIST_END

    /**
     * 创建文章
     */
    void createPost(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    /**
     * 获取文章列表
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

private:
    /**
     * 验证文章数据
     */
    bool validatePostData(const Json::Value& data, std::string& error) const;

    /**
     * 处理文章分类
     */
    void handleCategories(int articleId, const Json::Value& categories) const;

    /**
     * 处理文章标签
     */
    void handleTags(int articleId, const Json::Value& tags) const;

    void createArticleWithTransaction(DbManager& dbManager,
                                    const std::string& userUuid,
                                    const Json::Value& jsonBody,
                                    const std::function<void(const drogon::HttpResponsePtr&)>& callback) const;
}; 