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
        
        // 获取分页参数，默认每页10条，第1页
        int page = 1;
        int pageSize = 10;
        
        // 尝试从请求中获取分页参数
        try {
            auto params = req->getParameters();
            if (params.find("page") != params.end()) {
                page = std::stoi(params["page"]);
                if (page < 1) page = 1;
            }
            if (params.find("pageSize") != params.end()) {
                pageSize = std::stoi(params["pageSize"]);
                if (pageSize < 1) pageSize = 10;
                if (pageSize > 50) pageSize = 50; // 限制最大每页数量
            }
        } catch (const std::exception& e) {
            // 参数解析失败时使用默认值
            std::cerr << "解析分页参数失败: " << e.what() << std::endl;
        }
        
        // 计算偏移量
        int offset = (page - 1) * pageSize;
        
        // 检查文章是否存在
        dbManager.executeQuery(
            "SELECT id FROM articles WHERE id = " + std::to_string(article_id) + " AND is_published = true",
            [=, &dbManager, callback=callback](const drogon::orm::Result& articleResult) {
                if (articleResult.size() == 0) {
                    callback(utils::createErrorResponse(utils::ErrorCode::RESOURCE_NOT_FOUND, "文章不存在"));
                    return;
                }
                
                // 先查询总评论数
                std::string countSql = "SELECT COUNT(*) FROM comments WHERE article_id = " + std::to_string(article_id);
                
                dbManager.executeQuery(
                    countSql,
                    [=, &dbManager, callback=callback, page, pageSize, offset](const drogon::orm::Result& countResult) {
                        int total = countResult[0][0].as<int>();
                        int totalPages = (total + pageSize - 1) / pageSize; // 向上取整
                        
                        // 使用左连接查询评论及其引用的评论信息
                        std::string sql = 
                            "SELECT c.id, c.content, c.user_uuid, c.article_id, c.parent_id, "
                            "c.created_at, c.author_name, u.username as author_username, "
                            "p.content as parent_content, p.author_name as parent_author_name, "
                            "pu.username as parent_username "
                            "FROM comments c "
                            "LEFT JOIN users u ON c.user_uuid = u.uuid "
                            "LEFT JOIN comments p ON c.parent_id = p.id "
                            "LEFT JOIN users pu ON p.user_uuid = pu.uuid "
                            "WHERE c.article_id = " + std::to_string(article_id) + " "
                            "ORDER BY c.created_at DESC "
                            "LIMIT " + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);
                        
                        // 调试日志：输出SQL
                        std::cout << "执行评论查询SQL: " << sql << std::endl;
                        std::cout << "文章ID: " << article_id << ", 页码: " << page << ", 每页: " << pageSize << std::endl;
                        
                        dbManager.executeQuery(
                            sql,
                            [callback=callback, total, totalPages, page, pageSize](const drogon::orm::Result& result) {
                                Json::Value commentsArray(Json::arrayValue);
                                
                                // 调试日志：结果行数
                                std::cout << "查询到的评论数量: " << result.size() << std::endl;
                                
                                // 处理评论列表，平级结构
                                for (const auto& row : result) {
                                    Json::Value comment;
                                    comment["id"] = row["id"].as<int>();
                                    comment["content"] = row["content"].as<std::string>();
                                    
                                    // 设置作者信息
                                    if (!row["user_uuid"].isNull()) {
                                        comment["author"] = row["author_username"].as<std::string>();
                                        comment["isAuthenticated"] = true;
                                    } else {
                                        comment["author"] = row["author_name"].as<std::string>();
                                        comment["isAuthenticated"] = false;
                                    }
                                    
                                    // 添加父评论信息（用于显示引用）
                                    if (!row["parent_id"].isNull()) {
                                        int parentId = row["parent_id"].as<int>();
                                        comment["parent_id"] = parentId;
                                        
                                        // 构建父评论引用信息
                                        if (!row["parent_content"].isNull()) {
                                            // 确定父评论的作者名
                                            std::string parentAuthor;
                                            if (!row["parent_username"].isNull()) {
                                                parentAuthor = row["parent_username"].as<std::string>();
                                            } else if (!row["parent_author_name"].isNull()) {
                                                parentAuthor = row["parent_author_name"].as<std::string>();
                                            } else {
                                                parentAuthor = "匿名用户";
                                            }
                                            
                                            comment["parent_author"] = parentAuthor;
                                            
                                            // 获取父评论内容（仅截取前50个字符作为引用）
                                            std::string parentContent = row["parent_content"].as<std::string>();
                                            if (parentContent.length() > 50) {
                                                parentContent = parentContent.substr(0, 50) + "...";
                                            }
                                            comment["parent_content"] = parentContent;
                                        }
                                    } else {
                                        comment["parent_id"] = 0;
                                    }
                                    
                                    comment["created_at"] = row["created_at"].as<std::string>();
                                    commentsArray.append(comment);
                                }
                                
                                Json::Value responseData;
                                responseData["comments"] = commentsArray;
                                responseData["total"] = total;
                                responseData["page"] = page;
                                responseData["pageSize"] = pageSize;
                                responseData["totalPages"] = totalPages;
                                
                                callback(utils::createSuccessResponse("获取评论成功", responseData));
                            },
                            [callback=callback](const drogon::orm::DrogonDbException& e) {
                                std::cerr << "获取文章评论出错: " << e.base().what() << std::endl;
                                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                            }
                        );
                    },
                    [callback=callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "获取评论总数出错: " << e.base().what() << std::endl;
                        callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    }
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "检查文章存在性出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
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
            "SELECT id FROM articles WHERE id = " + std::to_string(articleId) + " AND is_published = true",
            [=, &dbManager, callback=callback](const drogon::orm::Result& articleResult) {
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
            }
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
            "SELECT id FROM articles WHERE id = " + std::to_string(articleId) + " AND is_published = true",
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
            }
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
                    }
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
                    responseData["message"] = "评论添加成功";
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
                    responseData["message"] = "评论添加成功";
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

/**
 * 获取所有评论列表
 */
void CommentController::getComments(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 从URL参数中获取分页信息
        int page = 1;
        int pageSize = 20;
        
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
        
        // 查询评论
        std::string sql = 
            "SELECT c.id, c.content, c.created_at, "
            "c.article_id, c.parent_id, a.title as article_title, "
            "COALESCE(u.username, c.author_name) as author_name, "
            "u.avatar as author_avatar, "
            "(SELECT COUNT(*) FROM comments) as total_count "
            "FROM comments c "
            "LEFT JOIN articles a ON c.article_id = a.id "
            "LEFT JOIN users u ON c.user_uuid = u.uuid "
            "ORDER BY c.created_at DESC "
            "LIMIT " + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);
        
        dbManager.executeQuery(
            sql,
            [callback=callback, page, pageSize](const drogon::orm::Result& result) {
                Json::Value commentsArray(Json::arrayValue);
                int totalCount = 0;
                
                for (const auto& row : result) {
                    Json::Value comment;
                    comment["id"] = row["id"].as<int>();
                    comment["content"] = row["content"].as<std::string>();
                    comment["created_at"] = row["created_at"].as<std::string>();
                    comment["article_id"] = row["article_id"].as<int>();
                    
                    if (!row["parent_id"].isNull()) {
                        comment["parent_id"] = row["parent_id"].as<int>();
                    }
                    
                    comment["article_title"] = row["article_title"].as<std::string>();
                    comment["author_name"] = row["author_name"].as<std::string>();
                    
                    if (!row["author_avatar"].isNull()) {
                        comment["author_avatar"] = row["author_avatar"].as<std::string>();
                    }
                    
                    commentsArray.append(comment);
                    
                    // 获取总记录数（所有行都有相同的值）
                    if (result.size() > 0 && totalCount == 0) {
                        totalCount = row["total_count"].as<int>();
                    }
                }
                
                // 计算总页数
                int totalPages = (totalCount + pageSize - 1) / pageSize;
                
                // 构建响应数据
                Json::Value responseData;
                responseData["comments"] = commentsArray;
                responseData["pagination"] = Json::Value();
                responseData["pagination"]["total"] = totalCount;
                responseData["pagination"]["page"] = page;
                responseData["pagination"]["pageSize"] = pageSize;
                responseData["pagination"]["totalPages"] = totalPages;
                
                callback(utils::createSuccessResponse("获取评论列表成功", responseData));
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                std::cerr << "获取评论列表出错: " << e.base().what() << std::endl;
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "获取评论列表异常: " << e.what() << std::endl;
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR));
    }
}

} // namespace v1
} // namespace api 