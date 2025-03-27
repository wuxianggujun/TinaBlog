#include "CommentController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/ErrorCode.hpp"
#include "blog/db/DbManager.hpp"
#include <iostream>

namespace api {
namespace v1 {

/**
 * 获取文章评论列表
 */
void CommentController::getArticleComments(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                        int article_id) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 检查文章是否存在
        dbManager.executeQuery(
            "SELECT id FROM articles WHERE id = $1 AND is_published = true",
            [=, &dbManager, callback=callback](const drogon::orm::Result& articleResult) {
                if (articleResult.size() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "文章不存在"));
                    return;
                }
                
                // 查询文章评论，以树状结构返回
                std::string sql = 
                    "WITH RECURSIVE comment_tree AS ("
                    "  SELECT c.id, c.content, c.user_uuid, c.article_id, c.parent_id, "
                    "         c.created_at, c.author_name, u.username, 0 AS level "
                    "  FROM comments c "
                    "  LEFT JOIN users u ON c.user_uuid = u.uuid "
                    "  WHERE c.article_id = $1 AND c.parent_id IS NULL "
                    "  UNION ALL "
                    "  SELECT c.id, c.content, c.user_uuid, c.article_id, c.parent_id, "
                    "         c.created_at, c.author_name, u.username, ct.level + 1 "
                    "  FROM comments c "
                    "  JOIN comment_tree ct ON c.parent_id = ct.id "
                    "  LEFT JOIN users u ON c.user_uuid = u.uuid "
                    ") "
                    "SELECT * FROM comment_tree "
                    "ORDER BY level, created_at DESC";
                
                dbManager.executeQuery(
                    sql,
                    [callback=callback](const drogon::orm::Result& result) {
                        Json::Value commentsArray(Json::arrayValue);
                        std::map<int, Json::Value> commentMap;
                        
                        // 构建评论树结构
                        for (const auto& row : result) {
                            Json::Value comment;
                            int id = row["id"].as<int>();
                            int parentId = row["parent_id"].isNull() ? 0 : row["parent_id"].as<int>();
                            
                            comment["id"] = id;
                            comment["content"] = row["content"].as<std::string>();
                            
                            // 设置作者信息
                            if (!row["user_uuid"].isNull()) {
                                comment["author"] = row["username"].as<std::string>();
                                comment["isAuthenticated"] = true;
                            } else {
                                comment["author"] = row["author_name"].as<std::string>();
                                comment["isAuthenticated"] = false;
                            }
                            
                            comment["created_at"] = row["created_at"].as<std::string>();
                            comment["replies"] = Json::Value(Json::arrayValue);
                            
                            // 放入评论映射
                            commentMap[id] = comment;
                            
                            // 将根评论添加到数组，或将子评论添加到父评论
                            if (parentId == 0) {
                                commentsArray.append(comment);
                            } else if (commentMap.find(parentId) != commentMap.end()) {
                                Json::Value& parentComment = commentMap[parentId];
                                parentComment["replies"].append(comment);
                            }
                        }
                        
                        Json::Value responseData;
                        responseData["comments"] = commentsArray;
                        responseData["total"] = commentsArray.size();
                        
                        callback(utils::createSuccessResponse("获取评论成功", responseData));
                    },
                    [callback=callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "获取文章评论出错: " << e.base().what() << std::endl;
                        callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    },
                    article_id
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "检查文章存在性出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            article_id
        );
    } catch (const std::exception& e) {
        std::cerr << "获取文章评论异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 添加评论（已登录用户）
 */
void CommentController::addComment(const drogon::HttpRequestPtr& req,
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

        // 验证评论数据
        std::string error;
        if (!validateCommentData(*jsonBody, error)) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, error));
            return;
        }
        
        // 检查必要字段
        if (!jsonBody->isMember("article_id") || !jsonBody->isMember("content")) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "缺少必要字段"));
            return;
        }
        
        int articleId = (*jsonBody)["article_id"].asInt();
        std::string content = (*jsonBody)["content"].asString();
        int parentId = jsonBody->isMember("parent_id") ? (*jsonBody)["parent_id"].asInt() : 0;
        
        auto& dbManager = DbManager::getInstance();
        
        // 检查文章是否存在
        dbManager.executeQuery(
            "SELECT id FROM articles WHERE id = $1 AND is_published = true",
            [=, &dbManager, callback=callback, userUuid](const drogon::orm::Result& articleResult) {
                if (articleResult.size() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "文章不存在"));
                    return;
                }
                
                // 检查父评论是否存在（如果有）
                if (parentId > 0) {
                    dbManager.executeQuery(
                        "SELECT id FROM comments WHERE id = $1 AND article_id = $2",
                        [=, &dbManager, callback=callback, userUuid](const drogon::orm::Result& parentResult) {
                            if (parentResult.size() == 0) {
                                callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "父评论不存在"));
                                return;
                            }
                            
                            // 添加评论
                            insertComment(dbManager, articleId, userUuid, content, parentId, "", "", callback);
                        },
                        [callback=callback](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "检查父评论存在性出错: " << e.base().what() << std::endl;
                            callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                        },
                        parentId, articleId
                    );
                } else {
                    // 添加评论（没有父评论）
                    insertComment(dbManager, articleId, userUuid, content, 0, "", "", callback);
                }
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "检查文章存在性出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            articleId
        );
    } catch (const std::exception& e) {
        std::cerr << "添加评论异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 添加匿名评论
 */
void CommentController::addAnonymousComment(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        // 解析请求体
        auto jsonBody = req->getJsonObject();
        if (!jsonBody) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_REQUEST));
            return;
        }

        // 验证评论数据
        std::string error;
        if (!validateCommentData(*jsonBody, error)) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, error));
            return;
        }
        
        // 检查必要字段
        if (!jsonBody->isMember("article_id") || !jsonBody->isMember("content") || 
            !jsonBody->isMember("author_name") || !jsonBody->isMember("author_email")) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "缺少必要字段"));
            return;
        }
        
        int articleId = (*jsonBody)["article_id"].asInt();
        std::string content = (*jsonBody)["content"].asString();
        std::string authorName = (*jsonBody)["author_name"].asString();
        std::string authorEmail = (*jsonBody)["author_email"].asString();
        int parentId = jsonBody->isMember("parent_id") ? (*jsonBody)["parent_id"].asInt() : 0;
        
        auto& dbManager = DbManager::getInstance();
        
        // 检查文章是否存在
        dbManager.executeQuery(
            "SELECT id FROM articles WHERE id = $1 AND is_published = true",
            [=, &dbManager, callback=callback](const drogon::orm::Result& articleResult) {
                if (articleResult.size() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "文章不存在"));
                    return;
                }
                
                // 检查父评论是否存在（如果有）
                if (parentId > 0) {
                    dbManager.executeQuery(
                        "SELECT id FROM comments WHERE id = $1 AND article_id = $2",
                        [=, &dbManager, callback=callback](const drogon::orm::Result& parentResult) {
                            if (parentResult.size() == 0) {
                                callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "父评论不存在"));
                                return;
                            }
                            
                            // 添加匿名评论
                            insertComment(dbManager, articleId, "", content, parentId, authorName, authorEmail, callback);
                        },
                        [callback=callback](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "检查父评论存在性出错: " << e.base().what() << std::endl;
                            callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                        },
                        parentId, articleId
                    );
                } else {
                    // 添加匿名评论（没有父评论）
                    insertComment(dbManager, articleId, "", content, 0, authorName, authorEmail, callback);
                }
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "检查文章存在性出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            articleId
        );
    } catch (const std::exception& e) {
        std::cerr << "添加匿名评论异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 回复评论
 */
void CommentController::replyComment(const drogon::HttpRequestPtr& req,
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

        // 验证评论内容
        if (!jsonBody->isMember("content") || (*jsonBody)["content"].asString().empty()) {
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "评论内容不能为空"));
            return;
        }
        
        std::string content = (*jsonBody)["content"].asString();
        auto& dbManager = DbManager::getInstance();
        
        // 检查父评论是否存在
        dbManager.executeQuery(
            "SELECT c.id, c.article_id FROM comments c WHERE c.id = $1",
            [=, &dbManager, callback=callback, userUuid](const drogon::orm::Result& result) {
                if (result.size() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "评论不存在"));
                    return;
                }
                
                int articleId = result[0]["article_id"].as<int>();
                
                // 检查文章是否存在
                dbManager.executeQuery(
                    "SELECT id FROM articles WHERE id = $1 AND is_published = true",
                    [=, &dbManager, callback=callback, userUuid](const drogon::orm::Result& articleResult) {
                        if (articleResult.size() == 0) {
                            callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "文章不存在"));
                            return;
                        }
                        
                        // 添加回复评论
                        insertComment(dbManager, articleId, userUuid, content, id, "", "", callback);
                    },
                    [callback=callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "检查文章存在性出错: " << e.base().what() << std::endl;
                        callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    },
                    articleId
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "检查父评论存在性出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            id
        );
    } catch (const std::exception& e) {
        std::cerr << "回复评论异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 删除评论
 */
void CommentController::deleteComment(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                    int id) const {
    try {
        // 获取当前用户UUID
        std::string userUuid = req->getAttributes()->get<std::string>("user_uuid");
        bool isAdmin = req->getAttributes()->get<bool>("is_admin");
        
        auto& dbManager = DbManager::getInstance();
        
        // 检查评论是否存在且属于当前用户
        std::string sql;
        if (isAdmin) {
            // 管理员可以删除任何评论
            sql = "DELETE FROM comments WHERE id = $1 RETURNING id";
        } else {
            // 普通用户只能删除自己的评论
            sql = "DELETE FROM comments WHERE id = $1 AND user_uuid = $2 RETURNING id";
        }
        
        if (isAdmin) {
            dbManager.executeQuery(
                sql,
                [callback=callback](const drogon::orm::Result& result) {
                    if (result.size() == 0) {
                        callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "评论不存在"));
                        return;
                    }
                    
                    Json::Value responseData;
                    responseData["message"] = "评论删除成功";
                    callback(utils::createSuccessResponse("评论删除成功", responseData));
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    std::cerr << "删除评论出错: " << e.base().what() << std::endl;
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_DELETE_ERROR));
                },
                id
            );
        } else {
            dbManager.executeQuery(
                sql,
                [callback=callback](const drogon::orm::Result& result) {
                    if (result.size() == 0) {
                        callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "评论不存在或无权限删除"));
                        return;
                    }
                    
                    Json::Value responseData;
                    responseData["message"] = "评论删除成功";
                    callback(utils::createSuccessResponse("评论删除成功", responseData));
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    std::cerr << "删除评论出错: " << e.base().what() << std::endl;
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_DELETE_ERROR));
                },
                id, userUuid
            );
        }
    } catch (const std::exception& e) {
        std::cerr << "删除评论异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

/**
 * 验证评论数据
 */
bool CommentController::validateCommentData(const Json::Value& data, std::string& error) const {
    if (!data.isMember("content") || data["content"].asString().empty()) {
        error = "评论内容不能为空";
        return false;
    }
    
    if (!data.isMember("article_id")) {
        error = "缺少文章ID";
        return false;
    }
    
    // 如果是匿名评论，检查作者信息
    if (data.isMember("author_name") && data.isMember("author_email")) {
        if (data["author_name"].asString().empty()) {
            error = "作者姓名不能为空";
            return false;
        }
        
        if (data["author_email"].asString().empty()) {
            error = "作者邮箱不能为空";
            return false;
        }
        
        // 简单的邮箱格式验证
        std::string email = data["author_email"].asString();
        if (email.find('@') == std::string::npos || email.find('.') == std::string::npos) {
            error = "邮箱格式不正确";
            return false;
        }
    }
    
    return true;
}

/**
 * 插入评论（辅助方法）
 */
void CommentController::insertComment(DbManager& dbManager, int articleId, const std::string& userUuid, 
                                   const std::string& content, int parentId,
                                   const std::string& authorName, const std::string& authorEmail,
                                   const std::function<void(const drogon::HttpResponsePtr&)>& callback) const {
    std::string sql;
    if (userUuid.empty()) {
        // 匿名评论
        if (parentId > 0) {
            sql = "INSERT INTO comments (content, article_id, parent_id, author_name, author_email, created_at) "
                  "VALUES ($1, $2, $3, $4, $5, NOW()) RETURNING id";
        } else {
            sql = "INSERT INTO comments (content, article_id, author_name, author_email, created_at) "
                  "VALUES ($1, $2, $3, $4, NOW()) RETURNING id";
        }
    } else {
        // 已登录用户评论
        if (parentId > 0) {
            sql = "INSERT INTO comments (content, article_id, user_uuid, parent_id, created_at) "
                  "VALUES ($1, $2, $3, $4, NOW()) RETURNING id";
        } else {
            sql = "INSERT INTO comments (content, article_id, user_uuid, created_at) "
                  "VALUES ($1, $2, $3, NOW()) RETURNING id";
        }
    }
    
    if (userUuid.empty()) {
        // 匿名评论
        if (parentId > 0) {
            dbManager.executeQuery(
                sql,
                [callback=callback](const drogon::orm::Result& result) {
                    Json::Value responseData;
                    responseData["id"] = result[0]["id"].as<int>();
                    responseData["message"] = "评论添加成功，等待审核";
                    callback(utils::createSuccessResponse("评论添加成功", responseData));
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    std::cerr << "添加评论出错: " << e.base().what() << std::endl;
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_INSERT_ERROR));
                },
                content, articleId, parentId, authorName, authorEmail
            );
        } else {
            dbManager.executeQuery(
                sql,
                [callback=callback](const drogon::orm::Result& result) {
                    Json::Value responseData;
                    responseData["id"] = result[0]["id"].as<int>();
                    responseData["message"] = "评论添加成功，等待审核";
                    callback(utils::createSuccessResponse("评论添加成功", responseData));
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    std::cerr << "添加评论出错: " << e.base().what() << std::endl;
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_INSERT_ERROR));
                },
                content, articleId, authorName, authorEmail
            );
        }
    } else {
        // 已登录用户评论
        if (parentId > 0) {
            dbManager.executeQuery(
                sql,
                [callback=callback](const drogon::orm::Result& result) {
                    Json::Value responseData;
                    responseData["id"] = result[0]["id"].as<int>();
                    responseData["message"] = "评论添加成功";
                    callback(utils::createSuccessResponse("评论添加成功", responseData));
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    std::cerr << "添加评论出错: " << e.base().what() << std::endl;
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_INSERT_ERROR));
                },
                content, articleId, userUuid, parentId
            );
        } else {
            dbManager.executeQuery(
                sql,
                [callback=callback](const drogon::orm::Result& result) {
                    Json::Value responseData;
                    responseData["id"] = result[0]["id"].as<int>();
                    responseData["message"] = "评论添加成功";
                    callback(utils::createSuccessResponse("评论添加成功", responseData));
                },
                [callback=callback](const drogon::orm::DrogonDbException& e) {
                    std::cerr << "添加评论出错: " << e.base().what() << std::endl;
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_INSERT_ERROR));
                },
                content, articleId, userUuid
            );
        }
    }
}

} // namespace v1
} // namespace api 