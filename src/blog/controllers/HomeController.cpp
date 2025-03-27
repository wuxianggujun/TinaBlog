#include "HomeController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/ErrorCode.hpp"
#include "blog/utils/ArticleUtils.hpp"
#include "blog/db/DbManager.hpp"
#include <iostream>

namespace api {
namespace v1 {

/**
 * 获取主页精选文章 
 */
void HomeController::getFeaturedArticles(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 简单实现：返回最新的5篇文章而不是阅读量最高的
        std::string sql = 
            "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
            "a.updated_at, a.is_published, u.username as author, "
            "(SELECT COUNT(*) FROM articles WHERE is_published = true) as total_count "
            "FROM articles a "
            "LEFT JOIN users u ON a.user_uuid = u.uuid "
            "WHERE a.is_published = true "
            "ORDER BY a.created_at DESC "  // 修改为按创建时间排序，而不是views
            "LIMIT 5";
        
        dbManager.executeQuery(
            sql,
            [callback=callback](const drogon::orm::Result& result) {
                Json::Value articles(Json::arrayValue);
                
                for (const auto& row : result) {
                    Json::Value article;
                    article["id"] = row["id"].as<int>();
                    article["title"] = row["title"].as<std::string>();
                    article["slug"] = row["slug"].as<std::string>();
                    
                    // 如果有摘要则使用摘要，否则截取内容的一部分
                    if (!row["summary"].isNull()) {
                        article["summary"] = row["summary"].as<std::string>();
                    } else {
                        std::string content = row["content"].as<std::string>();
                        article["summary"] = utils::ArticleUtils::generateSummary(content);
                    }
                    
                    article["created_at"] = row["created_at"].as<std::string>();
                    article["author"] = row["author"].as<std::string>();
                    
                    articles.append(article);
                }
                
                // 构建响应数据
                Json::Value responseData;
                responseData["articles"] = articles;
                responseData["isFeatured"] = true;
                
                callback(utils::createSuccessResponse("获取精选文章成功", responseData));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取精选文章出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取精选文章异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取最新文章
 */
void HomeController::getRecentArticles(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 从URL参数中获取分页信息
        int page = 1;
        int pageSize = 10;
        
        auto parameters = req->getParameters();
        if (parameters.find("page") != parameters.end()) {
            page = std::stoi(parameters["page"]);
            if (page < 1) page = 1;
        }
        
        if (parameters.find("pageSize") != parameters.end()) {
            pageSize = std::stoi(parameters["pageSize"]);
            if (pageSize < 1) pageSize = 10;
            if (pageSize > 50) pageSize = 50; // 限制最大页面大小
        }
        
        // 计算偏移量
        int offset = (page - 1) * pageSize;
        
        // 查询最新文章
        std::string sql = 
            "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
            "a.updated_at, a.is_published, u.username as author, "
            "(SELECT COUNT(*) FROM articles WHERE is_published = true) as total_count "
            "FROM articles a "
            "LEFT JOIN users u ON a.user_uuid = u.uuid "
            "WHERE a.is_published = true "
            "ORDER BY a.created_at DESC "
            "LIMIT ? OFFSET ?";
        
        dbManager.executeQuery(
            sql,
            [callback=callback, page, pageSize](const drogon::orm::Result& result) {
                callback(utils::createSuccessResponse("获取最新文章成功", 
                    utils::ArticleUtils::buildArticleResponse(result, page, pageSize)));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取最新文章出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            pageSize, offset
        );
    } catch (const std::exception& e) {
        std::cerr << "获取最新文章异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取分类下的文章
 */
void HomeController::getCategoryArticles(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                       const std::string& slug) const {
    try {
        if (slug.empty()) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "分类slug不能为空"));
            return;
        }
        
        auto& dbManager = DbManager::getInstance();
        
        // 从URL参数中获取分页信息
        int page = 1;
        int pageSize = 10;
        
        auto parameters = req->getParameters();
        if (parameters.find("page") != parameters.end()) {
            page = std::stoi(parameters["page"]);
            if (page < 1) page = 1;
        }
        
        if (parameters.find("pageSize") != parameters.end()) {
            pageSize = std::stoi(parameters["pageSize"]);
            if (pageSize < 1) pageSize = 10;
            if (pageSize > 50) pageSize = 50; // 限制最大页面大小
        }
        
        // 计算偏移量
        int offset = (page - 1) * pageSize;
        
        // 首先获取分类信息
        dbManager.executeQuery(
            "SELECT id, name FROM categories WHERE slug = ?",
            [=, &dbManager, callback=callback](const drogon::orm::Result& categoryResult) {
                if (categoryResult.size() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "未找到该分类"));
                    return;
                }
                
                int categoryId = categoryResult[0]["id"].as<int>();
                std::string categoryName = categoryResult[0]["name"].as<std::string>();
                
                // 查询该分类下的文章
                std::string sql = 
                    "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
                    "a.updated_at, a.is_published, u.username as author, "
                    "(SELECT COUNT(*) FROM articles a "
                    "JOIN article_categories ac ON a.id = ac.article_id "
                    "WHERE ac.category_id = ? AND a.is_published = true) as total_count "
                    "FROM articles a "
                    "JOIN article_categories ac ON a.id = ac.article_id "
                    "LEFT JOIN users u ON a.user_uuid = u.uuid "
                    "WHERE ac.category_id = ? AND a.is_published = true "
                    "ORDER BY a.created_at DESC "
                    "LIMIT ? OFFSET ?";
                
                dbManager.executeQuery(
                    sql,
                    [callback=callback, page, pageSize, categoryName](const drogon::orm::Result& result) {
                        Json::Value response = utils::ArticleUtils::buildArticleResponse(result, page, pageSize);
                        response["categoryName"] = categoryName;
                        callback(utils::createSuccessResponse("获取分类文章成功", response));
                    },
                    [callback=callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "获取分类文章出错: " << e.base().what() << std::endl;
                        callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    },
                    categoryId, categoryId, pageSize, offset
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取分类信息出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            slug
        );
    } catch (const std::exception& e) {
        std::cerr << "获取分类文章异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取标签下的文章
 */
void HomeController::getTagArticles(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                  const std::string& slug) const {
    try {
        if (slug.empty()) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "标签slug不能为空"));
            return;
        }
        
        auto& dbManager = DbManager::getInstance();
        
        // 从URL参数中获取分页信息
        int page = 1;
        int pageSize = 10;
        
        auto parameters = req->getParameters();
        if (parameters.find("page") != parameters.end()) {
            page = std::stoi(parameters["page"]);
            if (page < 1) page = 1;
        }
        
        if (parameters.find("pageSize") != parameters.end()) {
            pageSize = std::stoi(parameters["pageSize"]);
            if (pageSize < 1) pageSize = 10;
            if (pageSize > 50) pageSize = 50; // 限制最大页面大小
        }
        
        // 计算偏移量
        int offset = (page - 1) * pageSize;
        
        // 首先获取标签信息
        dbManager.executeQuery(
            "SELECT id, name FROM tags WHERE slug = ?",
            [=, &dbManager, callback=callback](const drogon::orm::Result& tagResult) {
                if (tagResult.size() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "未找到该标签"));
                    return;
                }
                
                int tagId = tagResult[0]["id"].as<int>();
                std::string tagName = tagResult[0]["name"].as<std::string>();
                
                // 查询该标签下的文章
                std::string sql = 
                    "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
                    "a.updated_at, a.is_published, u.username as author, "
                    "(SELECT COUNT(*) FROM articles a "
                    "JOIN article_tags at ON a.id = at.article_id "
                    "WHERE at.tag_id = ? AND a.is_published = true) as total_count "
                    "FROM articles a "
                    "JOIN article_tags at ON a.id = at.article_id "
                    "LEFT JOIN users u ON a.user_uuid = u.uuid "
                    "WHERE at.tag_id = ? AND a.is_published = true "
                    "ORDER BY a.created_at DESC "
                    "LIMIT ? OFFSET ?";
                
                dbManager.executeQuery(
                    sql,
                    [callback=callback, page, pageSize, tagName](const drogon::orm::Result& result) {
                        Json::Value response = utils::ArticleUtils::buildArticleResponse(result, page, pageSize);
                        response["tagName"] = tagName;
                        callback(utils::createSuccessResponse("获取标签文章成功", response));
                    },
                    [callback=callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "获取标签文章出错: " << e.base().what() << std::endl;
                        callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    },
                    tagId, tagId, pageSize, offset
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取标签信息出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            slug
        );
    } catch (const std::exception& e) {
        std::cerr << "获取标签文章异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取网站统计信息
 */
void HomeController::getSiteStats(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 并行查询各种统计信息
        auto stats = std::make_shared<Json::Value>();
        auto completedCount = std::make_shared<int>(0);
        const int totalQueries = 3; // 总共有三个查询
        
        // 查询文章总数
        dbManager.executeQuery(
            "SELECT COUNT(*) as article_count FROM articles WHERE is_published = true",
            [stats, completedCount, callback, totalQueries](const drogon::orm::Result& result) {
                (*stats)["articleCount"] = result[0]["article_count"].as<int>();
                
                (*completedCount)++;
                if (*completedCount == totalQueries) {
                    callback(utils::createSuccessResponse("获取网站统计成功", *stats));
                }
            },
            [callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取文章总数出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
        
        // 查询分类总数
        dbManager.executeQuery(
            "SELECT COUNT(*) as category_count FROM categories",
            [stats, completedCount, callback, totalQueries](const drogon::orm::Result& result) {
                (*stats)["categoryCount"] = result[0]["category_count"].as<int>();
                
                (*completedCount)++;
                if (*completedCount == totalQueries) {
                    callback(utils::createSuccessResponse("获取网站统计成功", *stats));
                }
            },
            [callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取分类总数出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
        
        // 查询标签总数
        dbManager.executeQuery(
            "SELECT COUNT(*) as tag_count FROM tags",
            [stats, completedCount, callback, totalQueries](const drogon::orm::Result& result) {
                (*stats)["tagCount"] = result[0]["tag_count"].as<int>();
                
                (*completedCount)++;
                if (*completedCount == totalQueries) {
                    callback(utils::createSuccessResponse("获取网站统计成功", *stats));
                }
            },
            [callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取标签总数出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取网站统计异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

} // namespace v1
} // namespace api 