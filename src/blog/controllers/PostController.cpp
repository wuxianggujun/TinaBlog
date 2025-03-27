#include "PostController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/ErrorCode.hpp"
#include "blog/utils/ArticleUtils.hpp"
#include "blog/db/DbManager.hpp"
#include <regex>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <atomic>
#include <thread>

namespace api {
namespace v1 {

/**
 * 创建文章
 */
void PostController::createPost(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 获取当前用户UUID
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        
        // 解析请求体
        auto jsonBody = req->getJsonObject();
        if (!jsonBody) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST));
            return;
        }

        // 验证文章数据
        std::string error;
        if (!validatePostData(*jsonBody, error)) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, error));
            return;
        }

        // 检查slug唯一性
        if (!(*jsonBody)["slug"].empty()) {
            auto& dbManager = DbManager::getInstance();
            dbManager.executeQuery(
                "SELECT COUNT(*) FROM articles WHERE user_uuid = $1 AND slug = $2",
                [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                    if (result[0]["count"].as<int>() > 0) {
                        callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "该文章链接已被使用"));
                        return;
                    }
                    
                    // 继续创建文章
                    createArticleWithTransaction(dbManager, userUuid, *jsonBody, callback);
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                },
                userUuid,
                (*jsonBody)["slug"].asString()
            );
        } else {
            // 如果没有提供slug，直接创建文章
            createArticleWithTransaction(DbManager::getInstance(), userUuid, *jsonBody, callback);
        }
    } catch (const std::exception& e) {
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
    }
}

/**
 * 获取文章列表
 */
void PostController::getArticles(const drogon::HttpRequestPtr& req,
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
        
        // 查询文章及其作者、分类信息，使用字符串拼接而非参数占位符
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
            [callback=callback, page, pageSize](const drogon::orm::Result& result) {
                callback(utils::createSuccessResponse("获取文章列表成功", 
                    utils::ArticleUtils::buildArticleResponse(result, page, pageSize)));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取文章列表出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取文章列表异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取单篇文章
 */
void PostController::getPost(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                           int id) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 获取文章基本信息
        dbManager.executeQuery(
            "SELECT a.*, u.username, u.display_name "
            "FROM articles a "
            "JOIN users u ON a.user_uuid = u.uuid "
            "WHERE a.id = $1",
            [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                if (result.empty()) {
                    callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "文章不存在"));
                    return;
                }
                
                // 获取文章分类
                dbManager.executeQuery(
                    "SELECT c.name FROM categories c "
                    "JOIN article_categories ac ON c.id = ac.category_id "
                    "WHERE ac.article_id = $1",
                    [=, &dbManager, callback=callback](const drogon::orm::Result& categories) {
                        // 获取文章标签
                        dbManager.executeQuery(
                            "SELECT t.name FROM tags t "
                            "JOIN article_tags at ON t.id = at.tag_id "
                            "WHERE at.article_id = $1",
                            [=, callback=callback](const drogon::orm::Result& tags) {
                                Json::Value article;
                                article["id"] = result[0]["id"].as<int>();
                                article["title"] = result[0]["title"].as<std::string>();
                                article["slug"] = result[0]["slug"].as<std::string>();
                                article["content"] = result[0]["content"].as<std::string>();
                                article["summary"] = result[0]["summary"].as<std::string>();
                                article["created_at"] = result[0]["created_at"].as<std::string>();
                                article["author"] = result[0]["display_name"].as<std::string>();
                                article["username"] = result[0]["username"].as<std::string>();
                                
                                // 添加分类
                                Json::Value categoryArray;
                                for (const auto& row : categories) {
                                    categoryArray.append(row["name"].as<std::string>());
                                }
                                article["categories"] = categoryArray;
                                
                                // 添加标签
                                Json::Value tagArray;
                                for (const auto& row : tags) {
                                    tagArray.append(row["name"].as<std::string>());
                                }
                                article["tags"] = tagArray;
                                
                                callback(utils::createSuccessResponse("获取文章成功", article));
                            },
                            [callback=callback](const drogon::orm::DrogonDbException& e) {
                                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                            },
                            id
                        );
                    },
                    [callback=callback](const drogon::orm::DrogonDbException& e) {
                        callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    },
                    id
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            id
        );
    } catch (const std::exception& e) {
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
    }
}

/**
 * 获取文章详情
 */
void PostController::getArticle(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                              const std::string& slug) const {
    try {
        // 使用共享指针追踪响应状态和线程安全保护，避免静态互斥锁
        auto responseGuard = std::make_shared<bool>(false);
        auto responseMutex = std::make_shared<std::mutex>();
        
        // 安全地发送响应的辅助函数
        auto sendResponse = [responseGuard, responseMutex, callback](const drogon::HttpResponsePtr& resp) {
            // 这里使用的是非静态互斥锁，避免死锁
            std::lock_guard<std::mutex> lock(*responseMutex);
            if (!*responseGuard) {
                *responseGuard = true;
                callback(resp);
            }
        };
        
        if (slug.empty()) {
            sendResponse(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "文章slug不能为空"));
            return;
        }
        
        auto& dbManager = DbManager::getInstance();
        
        // 设置超时处理，确保不会无限等待
        auto timeoutTimer = std::make_shared<std::thread>([responseGuard, responseMutex, callback]() {
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 5秒超时
            
            // 直接使用传入的callback和guard，避免使用sendResponse函数
            std::lock_guard<std::mutex> lock(*responseMutex);
            if (!*responseGuard) {
                *responseGuard = true;
                callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "获取文章超时，请稍后重试"));
            }
        });
        timeoutTimer->detach(); // 分离线程，允许它在后台运行
        
        // 查询文章基本信息
        try {
            dbManager.executeQuery(
                "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
                "a.updated_at, a.is_published, u.username as author, u.uuid as author_uuid, "
                "u.display_name as author_display_name, u.bio as author_bio "
                "FROM articles a "
                "LEFT JOIN users u ON a.user_uuid = u.uuid "
                "WHERE a.slug = $1 AND a.is_published = true",
                [this, sendResponse, responseGuard, responseMutex, &dbManager](const drogon::orm::Result& result) {
                    if (result.size() == 0) {
                        sendResponse(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "未找到该文章"));
                        return;
                    }
                    
                    try {
                        const auto& row = result[0];
                        Json::Value article;
                        int articleId = row["id"].as<int>();
                        std::string authorUuid = row["author_uuid"].as<std::string>();
                        
                        article["id"] = articleId;
                        article["title"] = row["title"].as<std::string>();
                        article["slug"] = row["slug"].as<std::string>();
                        article["summary"] = row["summary"].as<std::string>();
                        article["content"] = row["content"].as<std::string>();
                        article["created_at"] = row["created_at"].as<std::string>();
                        article["updated_at"] = row["updated_at"].as<std::string>();
                        article["is_published"] = row["is_published"].as<bool>();
                        article["author"] = row["author"].as<std::string>();
                        
                        // 创建共享指针用于跟踪完成状态
                        auto completed = std::make_shared<int>(0);
                        auto articleData = std::make_shared<Json::Value>(article);
                        
                        // 添加作者信息
                        Json::Value authorInfo;
                        authorInfo["username"] = row["author"].as<std::string>();
                        authorInfo["display_name"] = row["author_display_name"].as<std::string>();
                        authorInfo["bio"] = row["author_bio"].as<std::string>();
                        
                        // 获取作者文章数量、评论数量等统计信息
                        std::string authorStatsSql = 
                            "SELECT "
                            "(SELECT COUNT(*) FROM articles WHERE user_uuid = '" + authorUuid + "' AND is_published = true) as article_count, "
                            "(SELECT COALESCE(SUM(view_count), 0) FROM articles WHERE user_uuid = '" + authorUuid + "' AND is_published = true) as view_count, "
                            "(SELECT COUNT(*) FROM comments c JOIN articles a ON c.article_id = a.id WHERE a.user_uuid = '" + authorUuid + "') as comment_count";
                        
                        try {
                            // 创建一个副本用于lambda捕获
                            auto authorInfoPtr = std::make_shared<Json::Value>(authorInfo);
                            
                            dbManager.executeQuery(
                                authorStatsSql,
                                [completed, articleData, authorInfoPtr, sendResponse, responseGuard, responseMutex](const drogon::orm::Result& statsResult) {
                                    try {
                                        if (statsResult.size() > 0) {
                                            (*authorInfoPtr)["article_count"] = statsResult[0]["article_count"].as<int>();
                                            (*authorInfoPtr)["view_count"] = statsResult[0]["view_count"].as<int>();
                                            (*authorInfoPtr)["comment_count"] = statsResult[0]["comment_count"].as<int>();
                                        } else {
                                            (*authorInfoPtr)["article_count"] = 0;
                                            (*authorInfoPtr)["view_count"] = 0;
                                            (*authorInfoPtr)["comment_count"] = 0;
                                        }
                                        
                                        (*articleData)["author_info"] = *authorInfoPtr;
                                        
                                        // 增加完成计数
                                        (*completed)++;
                                        
                                        // 如果所有查询都完成，则返回结果
                                        if (*completed == 4) {
                                            Json::Value responseData;
                                            responseData["article"] = *articleData;
                                            sendResponse(utils::createSuccessResponse("获取文章详情成功", responseData));
                                        }
                                    } catch (const std::exception& e) {
                                        std::cerr << "处理作者统计信息时出错: " << e.what() << std::endl;
                                        sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                                    }
                                },
                                [sendResponse, responseGuard, responseMutex](const drogon::orm::DrogonDbException& e) {
                                    std::cerr << "获取作者统计信息出错: " << e.base().what() << std::endl;
                                    sendResponse(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                                }
                            );
                        } catch (const std::exception& e) {
                            std::cerr << "执行作者统计查询时出错: " << e.what() << std::endl;
                            sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                            return;
                        }
                        
                        // 查询作者其他文章（最新的3篇）
                        std::string authorArticlesSql = 
                            "SELECT id, title, slug, created_at "
                            "FROM articles "
                            "WHERE user_uuid = '" + authorUuid + "' AND id != " + std::to_string(articleId) + " AND is_published = true "
                            "ORDER BY created_at DESC "
                            "LIMIT 3";
                        
                        try {
                            dbManager.executeQuery(
                                authorArticlesSql,
                                [completed, articleData, sendResponse, responseMutex](const drogon::orm::Result& authorArticles) {
                                    try {
                                        Json::Value otherArticles(Json::arrayValue);
                                        
                                        for (const auto& artRow : authorArticles) {
                                            Json::Value otherArticle;
                                            otherArticle["id"] = artRow["id"].as<int>();
                                            otherArticle["title"] = artRow["title"].as<std::string>();
                                            otherArticle["slug"] = artRow["slug"].as<std::string>();
                                            otherArticle["created_at"] = artRow["created_at"].as<std::string>();
                                            otherArticles.append(otherArticle);
                                        }
                                        
                                        (*articleData)["other_author_articles"] = otherArticles;
                                        
                                        // 增加完成计数
                                        (*completed)++;
                                        
                                        // 如果所有查询都完成，则返回结果
                                        if (*completed == 4) {
                                            Json::Value responseData;
                                            responseData["article"] = *articleData;
                                            sendResponse(utils::createSuccessResponse("获取文章详情成功", responseData));
                                        }
                                    } catch (const std::exception& e) {
                                        std::cerr << "处理作者其他文章时出错: " << e.what() << std::endl;
                                        sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                                    }
                                },
                                [sendResponse, responseMutex](const drogon::orm::DrogonDbException& e) {
                                    std::cerr << "获取作者其他文章出错: " << e.base().what() << std::endl;
                                    sendResponse(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                                }
                            );
                        } catch (const std::exception& e) {
                            std::cerr << "执行作者其他文章查询时出错: " << e.what() << std::endl;
                            sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                            return;
                        }
                        
                        // 查询文章的分类
                        try {
                            dbManager.executeQuery(
                                "SELECT c.id, c.name, c.slug "
                                "FROM categories c "
                                "JOIN article_categories ac ON c.id = ac.category_id "
                                "WHERE ac.article_id = $1",
                                [this, completed, articleData, sendResponse, responseMutex, &dbManager, articleId](const drogon::orm::Result& catResult) {
                                    try {
                                        Json::Value categories(Json::arrayValue);
                                        std::string categoryId = "0";
                                        
                                        for (const auto& catRow : catResult) {
                                            Json::Value category;
                                            category["id"] = catRow["id"].as<int>();
                                            category["name"] = catRow["name"].as<std::string>();
                                            category["slug"] = catRow["slug"].as<std::string>();
                                            categories.append(category);
                                            
                                            // 使用第一个分类来查询相关文章
                                            if (categoryId == "0") {
                                                categoryId = std::to_string(catRow["id"].as<int>());
                                            }
                                        }
                                        
                                        (*articleData)["categories"] = categories;
                                        
                                        // 增加完成计数
                                        (*completed)++;
                                        
                                        // 查询同一分类的其他热门文章
                                        if (categoryId != "0") {
                                            try {
                                                std::string relatedArticlesSql = 
                                                    "SELECT a.id, a.title, a.slug, a.created_at "
                                                    "FROM articles a "
                                                    "JOIN article_categories ac ON a.id = ac.article_id "
                                                    "WHERE ac.category_id = " + categoryId + " AND a.id != " + std::to_string(articleId) + " AND a.is_published = true "
                                                    "ORDER BY a.view_count DESC, a.created_at DESC "
                                                    "LIMIT 3";
                                                
                                                dbManager.executeQuery(
                                                    relatedArticlesSql,
                                                    [completed, articleData, sendResponse, responseMutex](const drogon::orm::Result& relatedArticles) {
                                                        try {
                                                            Json::Value relatedArticlesArray(Json::arrayValue);
                                                            
                                                            for (const auto& artRow : relatedArticles) {
                                                                Json::Value relatedArticle;
                                                                relatedArticle["id"] = artRow["id"].as<int>();
                                                                relatedArticle["title"] = artRow["title"].as<std::string>();
                                                                relatedArticle["slug"] = artRow["slug"].as<std::string>();
                                                                relatedArticle["created_at"] = artRow["created_at"].as<std::string>();
                                                                relatedArticlesArray.append(relatedArticle);
                                                            }
                                                            
                                                            (*articleData)["related_articles"] = relatedArticlesArray;
                                                            
                                                            // 增加完成计数
                                                            (*completed)++;
                                                            
                                                            // 如果所有查询都完成，则返回结果
                                                            if (*completed == 4) {
                                                                Json::Value responseData;
                                                                responseData["article"] = *articleData;
                                                                sendResponse(utils::createSuccessResponse("获取文章详情成功", responseData));
                                                            }
                                                        } catch (const std::exception& e) {
                                                            std::cerr << "处理相关文章时出错: " << e.what() << std::endl;
                                                            sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                                                        }
                                                    },
                                                    [sendResponse, responseMutex](const drogon::orm::DrogonDbException& e) {
                                                        std::cerr << "获取相关文章出错: " << e.base().what() << std::endl;
                                                        sendResponse(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                                                    }
                                                );
                                            } catch (const std::exception& e) {
                                                std::cerr << "执行相关文章查询时出错: " << e.what() << std::endl;
                                                (*completed)++;
                                                if (*completed == 4) {
                                                    Json::Value responseData;
                                                    responseData["article"] = *articleData;
                                                    sendResponse(utils::createSuccessResponse("获取文章详情成功", responseData));
                                                }
                                            }
                                        } else {
                                            (*completed)++; // 如果没有分类，也要增加计数
                                            
                                            // 如果所有查询都完成，则返回结果
                                            if (*completed == 4) {
                                                Json::Value responseData;
                                                responseData["article"] = *articleData;
                                                sendResponse(utils::createSuccessResponse("获取文章详情成功", responseData));
                                            }
                                        }
                                    } catch (const std::exception& e) {
                                        std::cerr << "处理文章分类时出错: " << e.what() << std::endl;
                                        sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                                    }
                                },
                                [sendResponse, responseMutex](const drogon::orm::DrogonDbException& e) {
                                    std::cerr << "获取文章分类出错: " << e.base().what() << std::endl;
                                    sendResponse(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                                },
                                articleId
                            );
                        } catch (const std::exception& e) {
                            std::cerr << "执行文章分类查询时出错: " << e.what() << std::endl;
                            sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                            return;
                        }
                        
                        // 查询文章的标签
                        try {
                            dbManager.executeQuery(
                                "SELECT t.id, t.name, t.slug "
                                "FROM tags t "
                                "JOIN article_tags at ON t.id = at.tag_id "
                                "WHERE at.article_id = $1",
                                [completed, articleData, sendResponse, responseMutex](const drogon::orm::Result& tagResult) {
                                    try {
                                        Json::Value tags(Json::arrayValue);
                                        
                                        for (const auto& tagRow : tagResult) {
                                            Json::Value tag;
                                            tag["id"] = tagRow["id"].as<int>();
                                            tag["name"] = tagRow["name"].as<std::string>();
                                            tag["slug"] = tagRow["slug"].as<std::string>();
                                            tags.append(tag);
                                        }
                                        
                                        (*articleData)["tags"] = tags;
                                        
                                        // 增加完成计数
                                        (*completed)++;
                                        
                                        // 如果所有查询都完成，则返回结果
                                        if (*completed == 4) {
                                            Json::Value responseData;
                                            responseData["article"] = *articleData;
                                            sendResponse(utils::createSuccessResponse("获取文章详情成功", responseData));
                                        }
                                    } catch (const std::exception& e) {
                                        std::cerr << "处理文章标签时出错: " << e.what() << std::endl;
                                        sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                                    }
                                },
                                [sendResponse, responseMutex](const drogon::orm::DrogonDbException& e) {
                                    std::cerr << "获取文章标签出错: " << e.base().what() << std::endl;
                                    sendResponse(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                                },
                                articleId
                            );
                        } catch (const std::exception& e) {
                            std::cerr << "执行文章标签查询时出错: " << e.what() << std::endl;
                            sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                            return;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "处理文章基本信息时出错: " << e.what() << std::endl;
                        sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
                    }
                },
                [sendResponse, responseMutex](const drogon::orm::DrogonDbException& e) {
                    std::cerr << "获取文章详情出错: " << e.base().what() << std::endl;
                    sendResponse(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                },
                slug
            );
        } catch (const std::exception& e) {
            std::cerr << "执行文章基本查询时出错: " << e.what() << std::endl;
            sendResponse(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
        }
    } catch (const std::exception& e) {
        std::cerr << "获取文章详情异常: " << e.what() << std::endl;
        
        // 直接调用callback避免使用可能导致死锁的互斥锁
        auto errorResponse = utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what());
        callback(errorResponse);
    }
}

/**
 * 更新文章
 */
void PostController::updatePost(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                              int id) const {
    try {
        // 获取当前用户UUID
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        
        // 解析请求体
        auto jsonBody = req->getJsonObject();
        if (!jsonBody) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST));
            return;
        }

        // 验证文章数据
        std::string error;
        if (!validatePostData(*jsonBody, error)) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, error));
            return;
        }

        // 获取数据库连接
        auto& dbManager = DbManager::getInstance();

        // 开始事务
        dbManager.executeQuery("BEGIN", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});

        try {
            // 更新文章
            dbManager.executeQuery(
                "UPDATE articles SET title = $1, slug = $2, content = $3, summary = $4, "
                "is_published = $5, updated_at = NOW() "
                "WHERE id = $6 AND user_uuid = $7",
                [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                    if (result.affectedRows() == 0) {
                        // 回滚事务
                        dbManager.executeQuery("ROLLBACK", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});
                        callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "文章不存在或无权限修改"));
                        return;
                    }

                    // 删除旧的分类和标签关联
                    dbManager.executeQuery(
                        "DELETE FROM article_categories WHERE article_id = $1; "
                        "DELETE FROM article_tags WHERE article_id = $1",
                        [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {},
                        id
                    );

                    // 处理新的分类和标签
                    if ((*jsonBody)["categories"].isArray()) {
                        handleCategories(id, (*jsonBody)["categories"]);
                    }
                    if ((*jsonBody)["tags"].isArray()) {
                        handleTags(id, (*jsonBody)["tags"]);
                    }

                    // 提交事务
                    dbManager.executeQuery("COMMIT", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});

                    // 返回成功响应
                    Json::Value responseData;
                    responseData["id"] = id;
                    responseData["message"] = "文章更新成功";
                    callback(utils::createSuccessResponse("文章更新成功", responseData));
                },
                [=, &dbManager, callback=callback](const drogon::orm::DrogonDbException& e) {
                    // 回滚事务
                    dbManager.executeQuery("ROLLBACK", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_UPDATE_ERROR));
                },
                (*jsonBody)["title"].asString(),
                (*jsonBody)["slug"].asString(),
                (*jsonBody)["content"].asString(),
                (*jsonBody)["summary"].asString(),
                (*jsonBody)["published"].asBool(),
                id,
                userUuid
            );
        } catch (const std::exception& e) {
            // 回滚事务
            dbManager.executeQuery("ROLLBACK", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});
            throw e;
        }
    } catch (const std::exception& e) {
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
    }
}

/**
 * 删除文章
 */
void PostController::deletePost(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                              int id) const {
    try {
        // 获取当前用户UUID
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        
        auto& dbManager = DbManager::getInstance();
        
        // 删除文章（级联删除会自动处理关联的分类和标签）
        dbManager.executeQuery(
            "DELETE FROM articles WHERE id = $1 AND user_uuid = $2",
            [callback=callback](const drogon::orm::Result& result) {
                if (result.affectedRows() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "文章不存在或无权限删除"));
                    return;
                }
                
                Json::Value responseData;
                responseData["message"] = "文章删除成功";
                callback(utils::createSuccessResponse("文章删除成功", responseData));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                callback(utils::createErrorResponse(utils::ErrorCode::DB_DELETE_ERROR));
            },
            id,
            userUuid
        );
    } catch (const std::exception& e) {
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
    }
}

/**
 * 验证文章数据
 */
bool PostController::validatePostData(const Json::Value& data, std::string& error) const {
    // 检查必要字段
    if (!data.isMember("title") || data["title"].empty()) {
        error = "标题不能为空";
        return false;
    }
    
    if (!data.isMember("content") || data["content"].empty()) {
        error = "内容不能为空";
        return false;
    }
    
    // 验证slug格式（如果提供）
    if (data.isMember("slug") && !data["slug"].empty()) {
        std::string slug = data["slug"].asString();
        std::regex slugRegex("^[a-z0-9]+(?:-[a-z0-9]+)*$");
        if (!std::regex_match(slug, slugRegex)) {
            error = "文章链接格式不正确，只能包含小写字母、数字和连字符";
            return false;
        }
    }
    
    return true;
}

/**
 * 处理文章分类
 */
void PostController::handleCategories(int articleId, const Json::Value& categories) const {
    // 对每个分类进行处理
    for (const auto& category : categories) {
        if (!category.isString()) continue;
        
        std::string categoryName = category.asString();
        if (categoryName.empty()) continue;
        
        try {
            // 从分类名生成slug
            std::string slug = utils::ArticleUtils::generateSlug(categoryName);
            std::cout << "正在处理分类: " << categoryName << ", slug: " << slug << std::endl;
            
            // 首先查找分类是否已存在，使用精确匹配
            DbManager& dbManager = DbManager::getInstance();
            auto result = dbManager.execSyncQuery("SELECT id FROM categories WHERE name=$1", categoryName);
            
            int categoryId;
            if (result.size() > 0) {
                // 分类已存在，获取ID
                categoryId = result[0]["id"].as<int>();
                
                // 可能需要更新slug
                if (!slug.empty()) {
                    dbManager.executeQuery(
                        "UPDATE categories SET slug=$1 WHERE id=$2",
                        [](const drogon::orm::Result&) {}, 
                        [](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "更新分类slug失败: " << e.base().what() << std::endl;
                        },
                        slug, categoryId
                    );
                }
            } else {
                // 分类不存在，创建新分类 - 原样保存名称
                auto insertResult = dbManager.execSyncQuery(
                    "INSERT INTO categories (name, slug) VALUES ($1, $2) RETURNING id",
                    categoryName, slug
                );
                categoryId = insertResult[0]["id"].as<int>();
            }
            
            // 创建文章与分类的关联
            dbManager.executeQuery(
                "INSERT INTO article_categories (article_id, category_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
                [](const drogon::orm::Result&) {}, 
                [](const drogon::orm::DrogonDbException& e) {
                    std::cerr << "创建文章分类关联失败: " << e.base().what() << std::endl;
                },
                articleId, categoryId
            );
        } catch (const std::exception& e) {
            std::cerr << "处理分类时出错: " << e.what() << std::endl;
        }
    }
}

/**
 * 处理文章标签
 */
void PostController::handleTags(int articleId, const Json::Value& tags) const {
    // 对每个标签进行处理
    for (const auto& tag : tags) {
        if (!tag.isString()) continue;
        
        std::string tagName = tag.asString();
        if (tagName.empty()) continue;
        
        try {
            // 从标签名生成slug
            std::string slug = utils::ArticleUtils::generateSlug(tagName);
            std::cout << "正在处理标签: " << tagName << ", slug: " << slug << std::endl;
            
            // 首先查找标签是否已存在，使用精确匹配
            DbManager& dbManager = DbManager::getInstance();
            auto result = dbManager.execSyncQuery("SELECT id FROM tags WHERE name=$1", tagName);
            
            int tagId;
            if (result.size() > 0) {
                // 标签已存在，获取ID
                tagId = result[0]["id"].as<int>();
                
                // 可能需要更新slug
                if (!slug.empty()) {
                    dbManager.executeQuery(
                        "UPDATE tags SET slug=$1 WHERE id=$2",
                        [](const drogon::orm::Result&) {}, 
                        [](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "更新标签slug失败: " << e.base().what() << std::endl;
                        },
                        slug, tagId
                    );
                }
            } else {
                // 标签不存在，创建新标签 - 原样保存名称
                auto insertResult = dbManager.execSyncQuery(
                    "INSERT INTO tags (name, slug) VALUES ($1, $2) RETURNING id",
                    tagName, slug
                );
                tagId = insertResult[0]["id"].as<int>();
            }
            
            // 创建文章与标签的关联
            dbManager.executeQuery(
                "INSERT INTO article_tags (article_id, tag_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
                [](const drogon::orm::Result&) {}, 
                [](const drogon::orm::DrogonDbException& e) {
                    std::cerr << "创建文章标签关联失败: " << e.base().what() << std::endl;
                },
                articleId, tagId
            );
        } catch (const std::exception& e) {
            std::cerr << "处理标签时出错: " << e.what() << std::endl;
        }
    }
}

// 添加新的辅助方法
void PostController::createArticleWithTransaction(DbManager& dbManager,
                                                const std::string& userUuid,
                                                const Json::Value& jsonBody,
                                                const std::function<void(const drogon::HttpResponsePtr&)>& callback) const {
    // 开始事务
    dbManager.executeQuery("BEGIN", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});

    try {
        // 获取发布状态，默认为true
        bool isPublished = true;
        if (jsonBody.isMember("published") && jsonBody["published"].isBool()) {
            isPublished = jsonBody["published"].asBool();
        }
        
        // 准备摘要字段，如果为空则设为NULL
        std::string summary = "";
        if (jsonBody.isMember("summary") && !jsonBody["summary"].isNull()) {
            summary = jsonBody["summary"].asString();
        }
        
        // 准备slug字段
        std::string slug = "";
        if (jsonBody.isMember("slug") && !jsonBody["slug"].isNull()) {
            slug = jsonBody["slug"].asString();
        } else {
            // 如果没提供slug，从标题生成
            slug = utils::ArticleUtils::generateSlug(jsonBody["title"].asString());
        }
        
        // 更新JSON对象中的slug
        Json::Value updatedJsonBody = jsonBody;
        updatedJsonBody["slug"] = slug;

        // 检查必要字段
        if (!updatedJsonBody.isMember("title") || !updatedJsonBody.isMember("content")) {
            // 回滚事务
            dbManager.executeQuery("ROLLBACK", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "标题和内容不能为空"));
            return;
        }

        // 记录请求信息
        std::cout << "创建文章 - 请求字段:" << std::endl;
        std::cout << "  标题: " << updatedJsonBody["title"].asString() << std::endl;
        std::cout << "  摘要: " << summary << std::endl;
        std::cout << "  Slug: " << slug << std::endl;
        std::cout << "  发布状态: " << (isPublished ? "是" : "否") << std::endl;
        
        // 记录分类信息
        std::cout << "  分类信息: ";
        if (updatedJsonBody.isMember("categories")) {
            if (updatedJsonBody["categories"].isArray()) {
                std::cout << "数组，长度 " << updatedJsonBody["categories"].size() << std::endl;
            } else {
                std::cout << "非数组类型: " << updatedJsonBody["categories"].type() << std::endl;
            }
        } else {
            std::cout << "字段不存在" << std::endl;
        }
        
        // 记录标签信息
        std::cout << "  标签信息: ";
        if (updatedJsonBody.isMember("tags")) {
            if (updatedJsonBody["tags"].isArray()) {
                std::cout << "数组，长度 " << updatedJsonBody["tags"].size() << std::endl;
            } else {
                std::cout << "非数组类型: " << updatedJsonBody["tags"].type() << std::endl;
            }
        } else {
            std::cout << "字段不存在" << std::endl;
        }

        // 插入文章
        dbManager.executeQuery(
            "INSERT INTO articles (title, slug, content, summary, user_uuid, is_published, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, NOW(), NOW()) RETURNING id, slug",
            [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                int articleId = result[0]["id"].as<int>();
                std::string returnedSlug = result[0]["slug"].as<std::string>();

                // 准备响应数据
                Json::Value responseData;
                responseData["id"] = articleId;
                responseData["slug"] = returnedSlug;

                // 使用异步处理分类 - 只有当categories是数组且非空时
                if (updatedJsonBody.isMember("categories") && updatedJsonBody["categories"].isArray() && !updatedJsonBody["categories"].empty()) {
                    // 创建安全的回调函数
                    auto categoriesComplete = std::make_shared<std::function<void()>>([=, &dbManager]() {
                        // 分类处理完成，处理标签
                        if (updatedJsonBody.isMember("tags") && updatedJsonBody["tags"].isArray() && !updatedJsonBody["tags"].empty()) {
                            // 创建安全的标签处理完成回调
                            auto tagsComplete = std::make_shared<std::function<void()>>([=, &dbManager]() {
                                // 标签处理完成，提交事务并返回响应
                                try {
                                    dbManager.executeQuery(
                                        "COMMIT", 
                                        [](const drogon::orm::Result&) {
                                            std::cout << "事务提交成功" << std::endl;
                                        }, 
                                        [](const drogon::orm::DrogonDbException& e) {
                                            std::cerr << "事务提交失败: " << e.base().what() << std::endl;
                                        }
                                    );
                                    callback(utils::createSuccessResponse("文章创建成功", responseData));
                                } catch (const std::exception& e) {
                                    std::cerr << "提交事务或响应时异常: " << e.what() << std::endl;
                                    callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "服务器内部错误"));
                                }
                            });
                            
                            // 处理标签
                            handleTagsAsync(articleId, updatedJsonBody["tags"], (*tagsComplete));
                        } else {
                            // 没有标签需要处理，直接提交事务并返回响应
                            try {
                                dbManager.executeQuery(
                                    "COMMIT", 
                                    [](const drogon::orm::Result&) {
                                        std::cout << "事务提交成功" << std::endl;
                                    }, 
                                    [](const drogon::orm::DrogonDbException& e) {
                                        std::cerr << "事务提交失败: " << e.base().what() << std::endl;
                                    }
                                );
                                callback(utils::createSuccessResponse("文章创建成功", responseData));
                            } catch (const std::exception& e) {
                                std::cerr << "提交事务或响应时异常: " << e.what() << std::endl;
                                callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "服务器内部错误"));
                            }
                        }
                    });
                    
                    // 处理分类
                    handleCategoriesAsync(articleId, updatedJsonBody["categories"], (*categoriesComplete));
                } else if (updatedJsonBody.isMember("tags") && updatedJsonBody["tags"].isArray() && !updatedJsonBody["tags"].empty()) {
                    // 没有分类但有标签
                    // 创建安全的回调函数
                    auto tagsComplete = std::make_shared<std::function<void()>>([=, &dbManager]() {
                        // 标签处理完成，提交事务并返回响应
                        try {
                            dbManager.executeQuery(
                                "COMMIT", 
                                [](const drogon::orm::Result&) {
                                    std::cout << "事务提交成功" << std::endl;
                                }, 
                                [](const drogon::orm::DrogonDbException& e) {
                                    std::cerr << "事务提交失败: " << e.base().what() << std::endl;
                                }
                            );
                            callback(utils::createSuccessResponse("文章创建成功", responseData));
                        } catch (const std::exception& e) {
                            std::cerr << "提交事务或响应时异常: " << e.what() << std::endl;
                            callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "服务器内部错误"));
                        }
                    });
                    
                    // 处理标签
                    handleTagsAsync(articleId, updatedJsonBody["tags"], (*tagsComplete));
                } else {
                    // 既没有分类也没有标签，直接提交事务并返回响应
                    try {
                        dbManager.executeQuery(
                            "COMMIT", 
                            [](const drogon::orm::Result&) {
                                std::cout << "事务提交成功" << std::endl;
                            }, 
                            [](const drogon::orm::DrogonDbException& e) {
                                std::cerr << "事务提交失败: " << e.base().what() << std::endl;
                            }
                        );
                        callback(utils::createSuccessResponse("文章创建成功", responseData));
                    } catch (const std::exception& e) {
                        std::cerr << "提交事务或响应时异常: " << e.what() << std::endl;
                        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, "服务器内部错误"));
                    }
                }
            },
            [=, &dbManager, callback=callback](const drogon::orm::DrogonDbException& e) {
                // 回滚事务
                dbManager.executeQuery("ROLLBACK", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});
                std::string errorMsg = std::string("数据库错误: ") + e.base().what();
                callback(utils::createErrorResponse(utils::ErrorCode::DB_INSERT_ERROR, errorMsg));
            },
            updatedJsonBody["title"].asString(),
            slug,
            updatedJsonBody["content"].asString(),
            summary,
            userUuid,
            isPublished
        );
    } catch (const std::exception& e) {
        // 回滚事务
        dbManager.executeQuery("ROLLBACK", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
    }
}

/**
 * 异步处理文章分类
 */
void PostController::handleCategoriesAsync(int articleId, const Json::Value& categories, std::function<void()> finalCallback) const {
    if (!categories.isArray() || categories.empty()) {
        if (finalCallback) finalCallback();
        return;
    }
    
    auto& dbManager = DbManager::getInstance();
    auto categoryIter = std::make_shared<Json::Value::const_iterator>(categories.begin());
    auto endIter = categories.end();

    // 使用递归lambda处理每个分类
    std::shared_ptr<std::function<void()>> processNextCategory = 
        std::make_shared<std::function<void()>>();
    
    *processNextCategory = [=, &dbManager]() mutable {
        if (*categoryIter == endIter) {
            // 所有分类处理完毕
            if (finalCallback) finalCallback();
            return;
        }

        const auto& category = **categoryIter;
        ++(*categoryIter); // 移到下一个

        if (!category.isString()) {
            (*processNextCategory)(); // 跳过非字符串
            return;
        }

        std::string categoryName = category.asString();
        if (categoryName.empty()) {
            (*processNextCategory)(); // 跳过空字符串
            return;
        }

        // 从分类名生成slug，确保特殊字符和中文得到处理
        std::string slug = utils::ArticleUtils::generateSlug(categoryName);
        std::cout << "异步处理分类: " << categoryName << ", slug: " << slug << std::endl;

        // 1. 异步检查分类是否存在，使用精确匹配名称
        auto nextCategoryCallback = processNextCategory; // 创建本地副本
        dbManager.executeQuery(
            "SELECT id FROM categories WHERE name=$1",
            [=, &dbManager](const drogon::orm::Result& result) {
                if (!result.empty()) {
                    // 2a. 分类存在，获取ID
                    int categoryId = result[0]["id"].as<int>();
                    
                    // 3. 异步关联文章和分类
                    dbManager.executeQuery(
                        "INSERT INTO article_categories (article_id, category_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
                        [=](const drogon::orm::Result&) { 
                            // 关联成功，处理下一个分类
                            if (nextCategoryCallback) (*nextCategoryCallback)(); 
                        }, 
                        [=](const drogon::orm::DrogonDbException& e) { 
                            std::cerr << "创建文章分类关联失败: " << e.base().what() << std::endl;
                            if (nextCategoryCallback) (*nextCategoryCallback)(); // 出错也处理下一个
                        },
                        articleId, categoryId
                    );
                    
                    // 可选：确保slug已设置
                    if (!slug.empty()) {
                        // 使用简单的回调，避免可能的循环引用
                        auto dummyCallback = [](const drogon::orm::Result&) {};
                        auto errorCallback = [](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "更新分类slug失败: " << e.base().what() << std::endl;
                        };
                        
                        dbManager.executeQuery(
                            "UPDATE categories SET slug=$1 WHERE id=$2", 
                            dummyCallback, errorCallback,
                            slug, categoryId
                        );
                    }
                } else {
                    // 2b. 分类不存在，异步插入，原样保存名称
                    dbManager.executeQuery(
                        "INSERT INTO categories (name, slug) VALUES ($1, $2) RETURNING id",
                        [=, &dbManager](const drogon::orm::Result& insertResult) {
                            int categoryId = insertResult[0]["id"].as<int>();

                            // 3. 异步关联文章和分类
                            dbManager.executeQuery(
                                "INSERT INTO article_categories (article_id, category_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
                                [=](const drogon::orm::Result&) {
                                    if (nextCategoryCallback) (*nextCategoryCallback)(); // 处理下一个
                                },
                                [=](const drogon::orm::DrogonDbException& e) {
                                    std::cerr << "创建文章分类关联失败: " << e.base().what() << std::endl;
                                    if (nextCategoryCallback) (*nextCategoryCallback)(); // 出错也处理下一个
                                },
                                articleId, categoryId
                            );
                        },
                        [=](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "创建分类失败: " << e.base().what() << std::endl;
                            if (nextCategoryCallback) (*nextCategoryCallback)(); // 出错也处理下一个
                        },
                        categoryName, slug
                    );
                }
            },
            [=](const drogon::orm::DrogonDbException& e) {
                std::cerr << "查询分类失败: " << e.base().what() << std::endl;
                if (nextCategoryCallback) (*nextCategoryCallback)(); // 出错也处理下一个
            },
            categoryName
        );
    };

    // 启动处理第一个分类
    (*processNextCategory)();
}

/**
 * 异步处理文章标签
 */
void PostController::handleTagsAsync(int articleId, const Json::Value& tags, std::function<void()> finalCallback) const {
    if (!tags.isArray() || tags.empty()) {
        if (finalCallback) finalCallback();
        return;
    }
    
    auto& dbManager = DbManager::getInstance();
    auto tagIter = std::make_shared<Json::Value::const_iterator>(tags.begin());
    auto endIter = tags.end();

    // 使用递归lambda处理每个标签
    std::shared_ptr<std::function<void()>> processNextTag = 
        std::make_shared<std::function<void()>>();
    
    *processNextTag = [=, &dbManager]() mutable {
        if (*tagIter == endIter) {
            // 所有标签处理完毕
            if (finalCallback) finalCallback();
            return;
        }

        const auto& tag = **tagIter;
        ++(*tagIter); // 移到下一个

        if (!tag.isString()) {
            (*processNextTag)(); // 跳过非字符串
            return;
        }

        std::string tagName = tag.asString();
        if (tagName.empty()) {
            (*processNextTag)(); // 跳过空字符串
            return;
        }

        // 从标签名生成slug，确保特殊字符和中文得到处理
        std::string slug = utils::ArticleUtils::generateSlug(tagName);
        std::cout << "异步处理标签: " << tagName << ", slug: " << slug << std::endl;

        // 创建本地副本以避免循环引用
        auto nextTagCallback = processNextTag;

        // 1. 异步检查标签是否存在，使用精确匹配名称
        dbManager.executeQuery(
            "SELECT id FROM tags WHERE name=$1",
            [=, &dbManager](const drogon::orm::Result& result) {
                if (!result.empty()) {
                    // 2a. 标签存在，获取ID
                    int tagId = result[0]["id"].as<int>();

                    // 3. 异步关联文章和标签
                    dbManager.executeQuery(
                        "INSERT INTO article_tags (article_id, tag_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
                        [=](const drogon::orm::Result&) {
                            // 关联成功，处理下一个标签
                            if (nextTagCallback) (*nextTagCallback)();
                        },
                        [=](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "创建文章标签关联失败: " << e.base().what() << std::endl;
                            if (nextTagCallback) (*nextTagCallback)(); // 出错也处理下一个
                        },
                        articleId, tagId
                    );
                    
                    // 可选：确保slug已设置
                    if (!slug.empty()) {
                        // 使用简单的回调，避免可能的循环引用
                        auto dummyCallback = [](const drogon::orm::Result&) {};
                        auto errorCallback = [](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "更新标签slug失败: " << e.base().what() << std::endl;
                        };

                        dbManager.executeQuery(
                            "UPDATE tags SET slug=$1 WHERE id=$2",
                            dummyCallback, errorCallback,
                            slug, tagId
                        );
                    }
                } else {
                    // 2b. 标签不存在，异步插入，原样保存名称
                    dbManager.executeQuery(
                        "INSERT INTO tags (name, slug) VALUES ($1, $2) RETURNING id",
                        [=, &dbManager](const drogon::orm::Result& insertResult) {
                            int tagId = insertResult[0]["id"].as<int>();

                            // 3. 异步关联文章和标签
                            dbManager.executeQuery(
                                "INSERT INTO article_tags (article_id, tag_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
                                [=](const drogon::orm::Result&) {
                                    if (nextTagCallback) (*nextTagCallback)(); // 处理下一个
                                },
                                [=](const drogon::orm::DrogonDbException& e) {
                                    std::cerr << "创建文章标签关联失败: " << e.base().what() << std::endl;
                                    if (nextTagCallback) (*nextTagCallback)(); // 出错也处理下一个
                                },
                                articleId, tagId
                            );
                        },
                        [=](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "创建标签失败: " << e.base().what() << std::endl;
                            if (nextTagCallback) (*nextTagCallback)(); // 出错也处理下一个
                        },
                        tagName, slug
                    );
                }
            },
            [=](const drogon::orm::DrogonDbException& e) {
                std::cerr << "查询标签失败: " << e.base().what() << std::endl;
                if (nextTagCallback) (*nextTagCallback)(); // 出错也处理下一个
            },
            tagName
        );
    };

    // 启动处理第一个标签
    (*processNextTag)();
}

/**
 * 获取所有分类
 */
void PostController::getCategories(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 查询所有分类及其文章数
        dbManager.executeQuery(
            "SELECT c.id, c.name, c.slug, COUNT(ac.article_id) as count "
            "FROM categories c "
            "LEFT JOIN article_categories ac ON c.id = ac.category_id "
            "GROUP BY c.id, c.name, c.slug "
            "ORDER BY count DESC, c.name ASC",
            [callback=callback](const drogon::orm::Result& result) {
                Json::Value categories(Json::arrayValue);
                
                for (const auto& row : result) {
                    Json::Value category;
                    category["id"] = row["id"].as<int>();
                    category["name"] = row["name"].as<std::string>();
                    category["slug"] = row["slug"].as<std::string>();
                    category["count"] = row["count"].as<int>();
                    categories.append(category);
                }
                
                Json::Value responseData;
                responseData["categories"] = categories;
                
                callback(utils::createSuccessResponse("获取分类列表成功", responseData));
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

/**
 * 获取所有标签
 */
void PostController::getTags(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 查询所有标签及其文章数
        dbManager.executeQuery(
            "SELECT t.id, t.name, t.slug, COUNT(at.article_id) as count "
            "FROM tags t "
            "LEFT JOIN article_tags at ON t.id = at.tag_id "
            "GROUP BY t.id, t.name, t.slug "
            "ORDER BY count DESC, t.name ASC",
            [callback=callback](const drogon::orm::Result& result) {
                Json::Value tags(Json::arrayValue);
                
                for (const auto& row : result) {
                    Json::Value tag;
                    tag["id"] = row["id"].as<int>();
                    tag["name"] = row["name"].as<std::string>();
                    tag["slug"] = row["slug"].as<std::string>();
                    tag["count"] = row["count"].as<int>();
                    tags.append(tag);
                }
                
                Json::Value responseData;
                responseData["tags"] = tags;
                
                callback(utils::createSuccessResponse("获取标签列表成功", responseData));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取标签列表出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取标签列表异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 创建文章
 */
void PostController::createArticle(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 获取当前用户UUID
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        
        // 解析请求体
        auto jsonBody = req->getJsonObject();
        if (!jsonBody) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST));
            return;
        }

        // 验证文章数据
        std::string error;
        if (!validatePostData(*jsonBody, error)) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, error));
            return;
        }

        // 检查必要字段
        if (!jsonBody->isMember("title") || !jsonBody->isMember("content")) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "标题和内容不能为空"));
            return;
        }

        // 准备slug字段
        std::string slug = "";
        if (jsonBody->isMember("slug") && !(*jsonBody)["slug"].isNull()) {
            slug = (*jsonBody)["slug"].asString();
        } else {
            // 如果没提供slug，从标题生成
            slug = utils::ArticleUtils::generateSlug((*jsonBody)["title"].asString());
        }
        
        // 更新JSON对象中的slug
        (*jsonBody)["slug"] = slug;

        // 检查slug唯一性
        auto& dbManager = DbManager::getInstance();
        dbManager.executeQuery(
            "SELECT COUNT(*) FROM articles WHERE slug = $1",
            [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                if (result[0][0].as<int>() > 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "该文章链接已被使用"));
                    return;
                }
                
                // 使用事务创建文章
                createArticleWithTransaction(dbManager, userUuid, *jsonBody, callback);
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR, e.base().what()));
            },
            slug
        );
    } catch (const std::exception& e) {
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
    }
}

/**
 * 获取作者当前文章
 */
void PostController::getPosts(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 获取当前用户UUID
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        
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
        
        // 查询当前用户的文章
        std::string sql = 
            "SELECT a.id, a.title, a.slug, a.summary, a.content, a.created_at, "
            "a.updated_at, a.is_published, "
            "(SELECT COUNT(*) FROM articles WHERE user_uuid = $1) as total_count "
            "FROM articles a "
            "WHERE a.user_uuid = $1 "
            "ORDER BY a.created_at DESC "
            "LIMIT " + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);
        
        dbManager.executeQuery(
            sql,
            [callback=callback, page, pageSize](const drogon::orm::Result& result) {
                Json::Value articles(Json::arrayValue);
                int totalCount = 0;
                
                for (const auto& row : result) {
                    Json::Value article;
                    article["id"] = row["id"].as<int>();
                    article["title"] = row["title"].as<std::string>();
                    article["slug"] = row["slug"].as<std::string>();
                    article["summary"] = row["summary"].as<std::string>();
                    
                    // 提取内容的一小部分作为预览
                    std::string content = row["content"].as<std::string>();
                    article["preview"] = content.length() > 200 ? 
                        content.substr(0, 200) + "..." : content;
                    
                    article["created_at"] = row["created_at"].as<std::string>();
                    article["updated_at"] = row["updated_at"].as<std::string>();
                    article["is_published"] = row["is_published"].as<bool>();
                    
                    articles.append(article);
                    
                    // 获取总记录数（所有行都有相同的值）
                    if (result.size() > 0 && totalCount == 0) {
                        totalCount = row["total_count"].as<int>();
                    }
                }
                
                // 计算总页数
                int totalPages = (totalCount + pageSize - 1) / pageSize;
                
                // 构建响应数据
                Json::Value responseData;
                responseData["articles"] = articles;
                responseData["pagination"] = Json::Value();
                responseData["pagination"]["total"] = totalCount;
                responseData["pagination"]["page"] = page;
                responseData["pagination"]["pageSize"] = pageSize;
                responseData["pagination"]["totalPages"] = totalPages;
                
                callback(utils::createSuccessResponse("获取用户文章列表成功", responseData));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取用户文章列表出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            userUuid
        );
    } catch (const std::exception& e) {
        std::cerr << "获取文章列表异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取用户相关标签
 */
void PostController::getUserTags(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 获取当前用户UUID
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        if (userUuid.empty()) {
            callback(utils::createErrorResponse(utils::ErrorCode::UNAUTHORIZED, "用户未认证"));
            return;
        }
        
        auto& dbManager = DbManager::getInstance();
        
        // 查询与用户相关的标签及其使用次数
        std::string sql = 
            "SELECT t.id, t.name, t.slug, COUNT(at.article_id) as count "
            "FROM tags t "
            "JOIN article_tags at ON t.id = at.tag_id "
            "JOIN articles a ON at.article_id = a.id "
            "WHERE a.user_uuid = '" + userUuid + "' "
            "GROUP BY t.id, t.name, t.slug "
            "ORDER BY count DESC, t.name ASC "
            "LIMIT 20"; // 限制返回数量，防止数据过多
        
        dbManager.executeQuery(
            sql,
            [callback=callback](const drogon::orm::Result& result) {
                Json::Value tags(Json::arrayValue);
                
                for (const auto& row : result) {
                    Json::Value tag;
                    tag["id"] = row["id"].as<int>();
                    tag["name"] = row["name"].as<std::string>();
                    tag["slug"] = row["slug"].as<std::string>();
                    tag["count"] = row["count"].as<int>();
                    tags.append(tag);
                }
                
                Json::Value responseData;
                responseData["tags"] = tags;
                
                callback(utils::createSuccessResponse("获取用户标签成功", responseData));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取用户标签出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取用户标签异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

}  // 命名空间v1结束
}  // 命名空间api结束 