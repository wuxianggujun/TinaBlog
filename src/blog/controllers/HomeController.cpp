#include "HomeController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/ErrorCode.hpp"
#include "blog/utils/ArticleUtils.hpp"
#include "blog/db/DbManager.hpp"
#include <iostream>
#include <regex>
#include <thread>
#include <mutex>

namespace api {
namespace v1 {

/**
 * 创建带超时保护的执行环境
 * @param callback 原始回调函数
 * @param timeoutMessage 超时消息
 * @param timeoutSeconds 超时秒数
 * @param action 需要执行的操作函数
 */
template<typename F>
void executeWithTimeout(
    std::function<void(const drogon::HttpResponsePtr&)> callback,
    const std::string& timeoutMessage,
    int timeoutSeconds,
    F&& action) {
    // 使用共享指针追踪响应状态和线程安全保护
    auto responseGuard = std::make_shared<bool>(false);
    auto responseMutex = std::make_shared<std::mutex>();
    
    // 安全地发送响应的辅助函数
    auto sendResponse = [responseGuard, responseMutex, callback](const drogon::HttpResponsePtr& resp) {
        std::lock_guard<std::mutex> lock(*responseMutex);
        if (!*responseGuard) {
            *responseGuard = true;
            callback(resp);
        }
    };
    
    // 设置超时处理，确保不会无限等待
    auto timeoutTimer = std::make_shared<std::thread>([responseGuard, responseMutex, callback, timeoutMessage, timeoutSeconds]() {
        std::this_thread::sleep_for(std::chrono::seconds(timeoutSeconds));
        std::lock_guard<std::mutex> lock(*responseMutex);
        if (!*responseGuard) {
            *responseGuard = true;
            callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, timeoutMessage));
        }
    });
    timeoutTimer->detach(); // 分离线程，允许它在后台运行
    
    // 执行实际操作
    action(sendResponse);
}

/**
 * 获取主页精选文章 
 */
void HomeController::getFeaturedArticles(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        executeWithTimeout(
            callback,
            "获取精选文章超时，请稍后重试",
            5, // 5秒超时
            [&](auto sendResponse) {
                auto& dbManager = DbManager::getInstance();
                
                // 简单实现：返回最新的5篇文章而不是阅读量最高的
                std::string sql = 
                    "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
                    "a.updated_at, a.is_published, u.username as author, "
                    "(SELECT COUNT(*) FROM articles WHERE is_published = true) as total_count "
                    "FROM articles a "
                    "LEFT JOIN users u ON a.user_uuid = u.uuid "
                    "WHERE a.is_published = true "
                    "ORDER BY a.created_at DESC "
                    "LIMIT 5";  // 直接使用字面值5，不使用占位符
                
                dbManager.executeQuery(
                    sql,
                    [sendResponse](const drogon::orm::Result& result) {
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
                        
                        sendResponse(utils::createSuccessResponse("获取精选文章成功", responseData));
                    },
                    [sendResponse](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "获取精选文章出错: " << e.base().what() << std::endl;
                        sendResponse(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    }
                );
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
        executeWithTimeout(
            callback,
            "获取最新文章超时，请稍后重试",
            5, // 5秒超时
            [&](auto sendResponse) {
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
                
                // 构建不使用参数的SQL查询
                std::string sql = 
                    "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
                    "a.updated_at, a.is_published, u.username as author, "
                    "(SELECT COUNT(*) FROM articles WHERE is_published = true) as total_count "
                    "FROM articles a "
                    "LEFT JOIN users u ON a.user_uuid = u.uuid "
                    "WHERE a.is_published = true "
                    "ORDER BY a.created_at DESC "
                    "LIMIT " + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);
                
                dbManager.executeQuery(
                    sql,
                    [sendResponse, page, pageSize](const drogon::orm::Result& result) {
                        sendResponse(utils::createSuccessResponse("获取最新文章成功", 
                            utils::ArticleUtils::buildArticleResponse(result, page, pageSize)));
                    },
                    [sendResponse](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "获取最新文章出错: " << e.base().what() << std::endl;
                        sendResponse(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    }
                );
            }
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
        
        // 使用引号包裹slug，防止SQL注入
        std::string escapedSlug = "'" + slug + "'";
        escapedSlug = std::regex_replace(escapedSlug, std::regex("'"), "''");
        
        // 首先获取分类信息，不使用参数占位符
        std::string categorySql = "SELECT id, name FROM categories WHERE slug = '" + escapedSlug + "'";
        
        dbManager.executeQuery(
            categorySql,
            [=, &dbManager, callback=callback](const drogon::orm::Result& categoryResult) {
                if (categoryResult.size() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "未找到该分类"));
                    return;
                }
                
                int categoryId = categoryResult[0]["id"].as<int>();
                std::string categoryName = categoryResult[0]["name"].as<std::string>();
                
                // 构建SQL查询，不使用参数占位符
                std::string sql = 
                    "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
                    "a.updated_at, a.is_published, u.username as author, "
                    "(SELECT COUNT(*) FROM articles a "
                    "JOIN article_categories ac ON a.id = ac.article_id "
                    "WHERE ac.category_id = " + std::to_string(categoryId) + " AND a.is_published = true) as total_count "
                    "FROM articles a "
                    "JOIN article_categories ac ON a.id = ac.article_id "
                    "LEFT JOIN users u ON a.user_uuid = u.uuid "
                    "WHERE ac.category_id = " + std::to_string(categoryId) + " AND a.is_published = true "
                    "ORDER BY a.created_at DESC "
                    "LIMIT " + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);
                
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
                    }
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取分类信息出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
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
        
        // 使用引号包裹slug，防止SQL注入
        std::string escapedSlug = "'" + slug + "'";
        escapedSlug = std::regex_replace(escapedSlug, std::regex("'"), "''");
        
        // 首先获取标签信息，不使用参数占位符
        std::string tagSql = "SELECT id, name FROM tags WHERE slug = '" + escapedSlug + "'";
        
        dbManager.executeQuery(
            tagSql,
            [=, &dbManager, callback=callback](const drogon::orm::Result& tagResult) {
                if (tagResult.size() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "未找到该标签"));
                    return;
                }
                
                int tagId = tagResult[0]["id"].as<int>();
                std::string tagName = tagResult[0]["name"].as<std::string>();
                
                // 构建SQL查询，不使用参数占位符
                std::string sql = 
                    "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
                    "a.updated_at, a.is_published, u.username as author, "
                    "(SELECT COUNT(*) FROM articles a "
                    "JOIN article_tags at ON a.id = at.article_id "
                    "WHERE at.tag_id = " + std::to_string(tagId) + " AND a.is_published = true) as total_count "
                    "FROM articles a "
                    "JOIN article_tags at ON a.id = at.article_id "
                    "LEFT JOIN users u ON a.user_uuid = u.uuid "
                    "WHERE at.tag_id = " + std::to_string(tagId) + " AND a.is_published = true "
                    "ORDER BY a.created_at DESC "
                    "LIMIT " + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);
                
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
                    }
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取标签信息出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
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

/**
 * 获取归档文章列表
 */
void HomeController::getArchives(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 构建SQL查询，获取所有已发布文章按时间排序
        std::string sql = 
            "SELECT a.id, a.title, a.slug, a.summary, a.created_at, "
            "u.username as author "
            "FROM articles a "
            "LEFT JOIN users u ON a.user_uuid = u.uuid "
            "WHERE a.is_published = true "
            "ORDER BY a.created_at DESC";
        
        dbManager.executeQuery(
            sql,
            [callback=callback](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    callback(utils::createSuccessResponse("暂无归档文章", Json::Value(Json::arrayValue)));
                    return;
                }
                
                // 将结果转换为JSON数组
                Json::Value articlesArray(Json::arrayValue);
                
                // 遍历结果集构建文章列表
                for (const auto& row : result) {
                    Json::Value article;
                    article["id"] = row["id"].as<int>();
                    article["title"] = row["title"].as<std::string>();
                    article["slug"] = row["slug"].as<std::string>();
                    
                    if (!row["summary"].isNull()) {
                        article["summary"] = row["summary"].as<std::string>();
                    }
                    
                    if (!row["author"].isNull()) {
                        article["author"] = row["author"].as<std::string>();
                    }
                    
                    article["created_at"] = row["created_at"].as<std::string>();
                    
                    articlesArray.append(article);
                }
                
                // 返回成功响应
                callback(utils::createSuccessResponse("获取归档文章成功", articlesArray));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取归档文章出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取归档文章异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取所有分类列表(含文章数量)
 */
void HomeController::getAllCategories(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 构建SQL查询，查询分类及每个分类的文章数量
        std::string sql = 
            "SELECT c.id, c.name, c.slug, c.description, "
            "(SELECT COUNT(*) FROM article_categories ac "
            "JOIN articles a ON ac.article_id = a.id "
            "WHERE ac.category_id = c.id AND a.is_published = true) as article_count "
            "FROM categories c "
            "ORDER BY c.name ASC";
        
        dbManager.executeQuery(
            sql,
            [callback=callback](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    callback(utils::createSuccessResponse("暂无分类", Json::Value(Json::arrayValue)));
                    return;
                }
                
                // 将结果转换为JSON数组
                Json::Value categoriesArray(Json::arrayValue);
                
                // 遍历结果集构建分类列表
                for (const auto& row : result) {
                    Json::Value category;
                    category["id"] = row["id"].as<int>();
                    category["name"] = row["name"].as<std::string>();
                    category["slug"] = row["slug"].as<std::string>();
                    category["article_count"] = row["article_count"].as<int>();
                    
                    if (!row["description"].isNull()) {
                        category["description"] = row["description"].as<std::string>();
                    }
                    
                    categoriesArray.append(category);
                }
                
                // 返回成功响应
                callback(utils::createSuccessResponse("获取分类列表成功", categoriesArray));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取分类列表出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取分类列表异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

} // namespace v1
} // namespace api 