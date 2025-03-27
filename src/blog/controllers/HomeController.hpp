#pragma once
#include <drogon/HttpController.h>
#include <memory>
#include <vector>
#include <string>

namespace api {
namespace v1 {

/**
 * 首页控制器
 * 处理首页展示相关功能
 */
class HomeController : public drogon::HttpController<HomeController>
{
public:
    METHOD_LIST_BEGIN
    // 获取主页精选文章
    ADD_METHOD_TO(HomeController::getFeaturedArticles, "/api/home/featured", drogon::Get);
    // 获取主页最新文章
    ADD_METHOD_TO(HomeController::getRecentArticles, "/api/home/recent", drogon::Get);
    // 获取分类文章列表
    ADD_METHOD_TO(HomeController::getCategoryArticles, "/api/category/{slug}", drogon::Get);
    // 获取标签文章列表
    ADD_METHOD_TO(HomeController::getTagArticles, "/api/tag/{slug}", drogon::Get);
    // 网站统计信息
    ADD_METHOD_TO(HomeController::getSiteStats, "/api/stats", drogon::Get);
    METHOD_LIST_END

    /**
     * 获取主页精选文章
     */
    void getFeaturedArticles(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    /**
     * 获取主页最新文章
     */
    void getRecentArticles(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    /**
     * 获取分类下的文章
     */
    void getCategoryArticles(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                           const std::string& slug) const;

    /**
     * 获取标签下的文章
     */
    void getTagArticles(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       const std::string& slug) const;

    /**
     * 获取网站统计信息
     */
    void getSiteStats(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
};

} // namespace v1
} // namespace api 