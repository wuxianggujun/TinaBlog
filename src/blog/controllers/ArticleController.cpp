#include "ArticleController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/ErrorCode.hpp"
#include <drogon/HttpResponse.h>
#include <iostream>

namespace api {
namespace v1 {

// 辅助函数声明
static void getArticleTags(DbManager& dbManager, int articleId, Json::Value& article);
static void getArticleCategories(DbManager& dbManager, int articleId, Json::Value& article);

/**
 * 获取当前用户的文章列表
 */
void ArticleController::getMyArticles(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 获取用户UUID
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
        
        // 构建主查询SQL
        std::string countSql = "SELECT COUNT(*) FROM articles WHERE user_uuid = '" + userUuid + "'";
        
        // 先获取总记录数
        dbManager.executeQuery(
            countSql,
            [=, &dbManager, callback=callback](const drogon::orm::Result& countResult) {
                int total = countResult[0][0].as<int>();
                int totalPages = (total + pageSize - 1) / pageSize; // 向上取整
                
                // 构建文章查询SQL
                std::string sql = 
                    "SELECT a.id, a.title, a.slug, a.summary, a.content, a.cover_image, "
                    "a.is_published, a.views, a.created_at, a.updated_at, "
                    "u.username as author_name, u.display_name, "
                    "(SELECT COUNT(*) FROM comments c WHERE c.article_id = a.id) as comment_count "
                    "FROM articles a "
                    "LEFT JOIN users u ON a.user_uuid = u.uuid "
                    "WHERE a.user_uuid = '" + userUuid + "' "
                    "ORDER BY a.created_at DESC "
                    "LIMIT " + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);
                
                dbManager.executeQuery(
                    sql,
                    [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                        Json::Value articlesArray(Json::arrayValue);
                        
                        // 处理每篇文章
                        for (const auto& row : result) {
                            Json::Value article;
                            article["id"] = row["id"].as<int>();
                            article["title"] = row["title"].as<std::string>();
                            article["slug"] = row["slug"].as<std::string>();
                            
                            // 处理可能为NULL的字段
                            if (!row["summary"].isNull())
                                article["summary"] = row["summary"].as<std::string>();
                            
                            if (!row["cover_image"].isNull())
                                article["cover_image"] = row["cover_image"].as<std::string>();
                            
                            article["is_published"] = row["is_published"].as<bool>();
                            article["views"] = row["views"].as<int>();
                            article["created_at"] = row["created_at"].as<std::string>();
                            article["updated_at"] = row["updated_at"].as<std::string>();
                            article["author_name"] = row["author_name"].as<std::string>();
                            
                            if (!row["display_name"].isNull())
                                article["author_display_name"] = row["display_name"].as<std::string>();
                            
                            article["comment_count"] = row["comment_count"].as<int>();
                            
                            // 获取文章标签
                            getArticleTags(dbManager, row["id"].as<int>(), article);
                            
                            // 获取文章分类
                            getArticleCategories(dbManager, row["id"].as<int>(), article);
                            
                            articlesArray.append(article);
                        }
                        
                        // 构建分页信息
                        Json::Value pagination;
                        pagination["total"] = total;
                        pagination["page"] = page;
                        pagination["pageSize"] = pageSize;
                        pagination["totalPages"] = totalPages;
                        
                        // 构建响应数据
                        Json::Value responseData;
                        responseData["articles"] = articlesArray;
                        responseData["pagination"] = pagination;
                        
                        callback(utils::createSuccessResponse("获取文章列表成功", articlesArray));
                    },
                    [callback=callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "获取文章列表出错: " << e.base().what() << std::endl;
                        callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    }
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取文章总数出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取个人文章列表异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 获取文章统计信息
 */
void ArticleController::getArticleStats(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 获取用户UUID
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        
        auto& dbManager = DbManager::getInstance();
        
        // 构建统计SQL
        std::string sql = 
            "SELECT "
            "COUNT(*) as total_count, "
            "SUM(CASE WHEN is_published = true THEN 1 ELSE 0 END) as published_count, "
            "SUM(CASE WHEN is_published = false THEN 1 ELSE 0 END) as draft_count, "
            "SUM(views) as total_views "
            "FROM articles "
            "WHERE user_uuid = '" + userUuid + "'";
        
        dbManager.executeQuery(
            sql,
            [callback=callback](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    Json::Value statsData;
                    statsData["total_count"] = 0;
                    statsData["published_count"] = 0;
                    statsData["draft_count"] = 0;
                    statsData["total_views"] = 0;
                    
                    callback(utils::createSuccessResponse("获取文章统计成功", statsData));
                    return;
                }
                
                Json::Value statsData;
                statsData["total_count"] = result[0]["total_count"].as<int>();
                statsData["published_count"] = result[0]["published_count"].as<int>();
                statsData["draft_count"] = result[0]["draft_count"].as<int>();
                statsData["total_views"] = result[0]["total_views"].as<int>();
                
                callback(utils::createSuccessResponse("获取文章统计成功", statsData));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取文章统计出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取文章统计异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

// 辅助方法：获取文章标签
static void getArticleTags(DbManager& dbManager, int articleId, Json::Value& article) {
    try {
        std::string tagSql = 
            "SELECT t.id, t.name, t.slug FROM tags t "
            "JOIN article_tags at ON t.id = at.tag_id "
            "WHERE at.article_id = " + std::to_string(articleId);
        
        auto tagResult = dbManager.execSyncQuery(tagSql);
        
        Json::Value tagsArray(Json::arrayValue);
        for (const auto& tagRow : tagResult) {
            Json::Value tag;
            tag["id"] = tagRow["id"].as<int>();
            tag["name"] = tagRow["name"].as<std::string>();
            tag["slug"] = tagRow["slug"].as<std::string>();
            tagsArray.append(tag);
        }
        
        article["tags"] = tagsArray;
    } catch (const std::exception& e) {
        // 如果获取标签失败，记录错误但继续处理
        std::cerr << "获取文章 " << articleId << " 的标签失败: " << e.what() << std::endl;
        article["tags"] = Json::Value(Json::arrayValue);
    }
}

// 辅助方法：获取文章分类
static void getArticleCategories(DbManager& dbManager, int articleId, Json::Value& article) {
    try {
        std::string categorySql = 
            "SELECT c.id, c.name, c.slug FROM categories c "
            "JOIN article_categories ac ON c.id = ac.category_id "
            "WHERE ac.article_id = " + std::to_string(articleId);
        
        auto categoryResult = dbManager.execSyncQuery(categorySql);
        
        Json::Value categoriesArray(Json::arrayValue);
        for (const auto& categoryRow : categoryResult) {
            Json::Value category;
            category["id"] = categoryRow["id"].as<int>();
            category["name"] = categoryRow["name"].as<std::string>();
            category["slug"] = categoryRow["slug"].as<std::string>();
            categoriesArray.append(category);
        }
        
        article["categories"] = categoriesArray;
    } catch (const std::exception& e) {
        // 如果获取分类失败，记录错误但继续处理
        std::cerr << "获取文章 " << articleId << " 的分类失败: " << e.what() << std::endl;
        article["categories"] = Json::Value(Json::arrayValue);
    }
}

} // namespace v1
} // namespace api 