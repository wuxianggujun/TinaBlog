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
 * 用户文章控制器
 * 专门处理用户自己文章的查询、管理等操作
 */
class ArticleController : public drogon::HttpController<ArticleController>
{
public:
    METHOD_LIST_BEGIN
    // 获取当前用户的文章列表
    ADD_METHOD_TO(ArticleController::getMyArticles, "/api/articles/my", drogon::Get, "JwtAuthFilter");
    // 添加新的路由，指向相同的处理器
    ADD_METHOD_TO(ArticleController::getMyArticles, "/api/user/articles", drogon::Get, "JwtAuthFilter");
    // 获取文章统计信息
    ADD_METHOD_TO(ArticleController::getArticleStats, "/api/articles/stats", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    /**
     * 获取当前用户的文章列表
     */
    void getMyArticles(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    
    /**
     * 获取文章统计信息
     */
    void getArticleStats(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
};

} // namespace v1
} // namespace api 