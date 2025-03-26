#include "PostController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/ErrorCode.hpp"
#include "blog/db/DbManager.hpp"
#include <regex>

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

        // 获取数据库连接
        auto& dbManager = DbManager::getInstance();

        // 开始事务
        dbManager.executeQuery("BEGIN", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});

        try {
            // 插入文章
            dbManager.executeQuery(
                "INSERT INTO articles (title, slug, content, summary, user_uuid, is_published, created_at, updated_at) "
                "VALUES ($1, $2, $3, $4, $5, $6, NOW(), NOW()) RETURNING id",
                [=, &dbManager, callback=callback](const drogon::orm::Result& result) {
                    int articleId = result[0]["id"].as<int>();

                    // 处理分类
                    if ((*jsonBody)["categories"].isArray()) {
                        handleCategories(articleId, (*jsonBody)["categories"]);
                    }

                    // 处理标签
                    if ((*jsonBody)["tags"].isArray()) {
                        handleTags(articleId, (*jsonBody)["tags"]);
                    }

                    // 提交事务
                    dbManager.executeQuery("COMMIT", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});

                    // 返回成功响应
                    Json::Value responseData;
                    responseData["id"] = articleId;
                    responseData["message"] = "文章创建成功";
                    callback(utils::createSuccessResponse("文章创建成功", responseData));
                },
                [=, &dbManager, callback=callback](const drogon::orm::DrogonDbException& e) {
                    // 回滚事务
                    dbManager.executeQuery("ROLLBACK", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});
                    callback(utils::createErrorResponse(utils::ErrorCode::DB_INSERT_ERROR));
                },
                (*jsonBody)["title"].asString(),
                (*jsonBody)["slug"].asString(),
                (*jsonBody)["content"].asString(),
                (*jsonBody)["summary"].asString(),
                userUuid,
                (*jsonBody)["published"].asBool()
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
 * 获取文章列表
 */
void PostController::getPosts(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        auto& dbManager = DbManager::getInstance();
        
        // 构建查询条件
        std::string whereClause = "WHERE is_published = true";
        std::vector<std::string> params;
        
        // 获取查询参数
        std::string page = req->getParameter("page");
        std::string limit = req->getParameter("limit");
        std::string category = req->getParameter("category");
        std::string tag = req->getParameter("tag");
        
        // 添加分类过滤
        if (!category.empty()) {
            whereClause += " AND EXISTS (SELECT 1 FROM article_categories ac "
                          "JOIN categories c ON ac.category_id = c.id "
                          "WHERE ac.article_id = articles.id AND c.name = $1)";
            params.push_back(category);
        }
        
        // 添加标签过滤
        if (!tag.empty()) {
            whereClause += " AND EXISTS (SELECT 1 FROM article_tags at "
                          "JOIN tags t ON at.tag_id = t.id "
                          "WHERE at.article_id = articles.id AND t.name = $2)";
            params.push_back(tag);
        }
        
        // 构建分页参数
        int pageNum = !page.empty() ? std::stoi(page) : 1;
        int pageSize = !limit.empty() ? std::stoi(limit) : 10;
        int offset = (pageNum - 1) * pageSize;
        
        // 获取总记录数
        std::string countSql = "SELECT COUNT(*) FROM articles " + whereClause;
        dbManager.executeQuery(
            countSql,
            [=, &dbManager, callback=callback](const drogon::orm::Result& countResult) {
                int total = countResult[0]["count"].as<int>();
                
                // 获取文章列表
                std::string sql = "SELECT a.*, u.username, u.display_name "
                                "FROM articles a "
                                "JOIN users u ON a.user_uuid = u.uuid "
                                + whereClause +
                                " ORDER BY a.created_at DESC LIMIT $3 OFFSET $4";
                
                dbManager.executeQuery(
                    sql,
                    [=, callback=callback](const drogon::orm::Result& result) {
                        Json::Value articles;
                        for (const auto& row : result) {
                            Json::Value article;
                            article["id"] = row["id"].as<int>();
                            article["title"] = row["title"].as<std::string>();
                            article["slug"] = row["slug"].as<std::string>();
                            article["summary"] = row["summary"].as<std::string>();
                            article["content"] = row["content"].as<std::string>();
                            article["created_at"] = row["created_at"].as<std::string>();
                            article["author"] = row["display_name"].as<std::string>();
                            article["username"] = row["username"].as<std::string>();
                            articles.append(article);
                        }
                        
                        Json::Value responseData;
                        responseData["articles"] = articles;
                        responseData["total"] = total;
                        responseData["page"] = pageNum;
                        responseData["page_size"] = pageSize;
                        
                        callback(utils::createSuccessResponse("获取文章列表成功", responseData));
                    },
                    [callback=callback](const drogon::orm::DrogonDbException& e) {
                        callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
                    },
                    params[0], params[1], pageSize, offset
                );
            },
            [callback=callback](const drogon::orm::DrogonDbException& e) {
                callback(utils::createErrorResponse(utils::ErrorCode::DB_QUERY_ERROR));
            },
            params[0], params[1]
        );
    } catch (const std::exception& e) {
        callback(utils::createErrorResponse(utils::ErrorCode::SERVER_ERROR, e.what()));
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
    auto& dbManager = DbManager::getInstance();
    
    for (const auto& category : categories) {
        std::string categoryName = category.asString();
        
        // 检查分类是否存在
        dbManager.executeQuery(
            "SELECT id FROM categories WHERE name = $1",
            [=, &dbManager](const drogon::orm::Result& result) {
                if (result.empty()) {
                    // 创建新分类
                    dbManager.executeQuery(
                        "INSERT INTO categories (name) VALUES ($1) RETURNING id",
                        [=, &dbManager](const drogon::orm::Result& newCategory) {
                            int categoryId = newCategory[0]["id"].as<int>();
                            
                            // 创建文章分类关联
                            dbManager.executeQuery(
                                "INSERT INTO article_categories (article_id, category_id) VALUES ($1, $2)",
                                [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {},
                                articleId,
                                categoryId
                            );
                        },
                        [](const drogon::orm::DrogonDbException&) {},
                        categoryName
                    );
                } else {
                    // 使用现有分类
                    int categoryId = result[0]["id"].as<int>();
                    
                    // 创建文章分类关联
                    dbManager.executeQuery(
                        "INSERT INTO article_categories (article_id, category_id) VALUES ($1, $2)",
                        [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {},
                        articleId,
                        categoryId
                    );
                }
            },
            [](const drogon::orm::DrogonDbException&) {},
            categoryName
        );
    }
}

/**
 * 处理文章标签
 */
void PostController::handleTags(int articleId, const Json::Value& tags) const {
    auto& dbManager = DbManager::getInstance();
    
    for (const auto& tag : tags) {
        std::string tagName = tag.asString();
        
        // 检查标签是否存在
        dbManager.executeQuery(
            "SELECT id FROM tags WHERE name = $1",
            [=, &dbManager](const drogon::orm::Result& result) {
                if (result.empty()) {
                    // 创建新标签
                    dbManager.executeQuery(
                        "INSERT INTO tags (name) VALUES ($1) RETURNING id",
                        [=, &dbManager](const drogon::orm::Result& newTag) {
                            int tagId = newTag[0]["id"].as<int>();
                            
                            // 创建文章标签关联
                            dbManager.executeQuery(
                                "INSERT INTO article_tags (article_id, tag_id) VALUES ($1, $2)",
                                [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {},
                                articleId,
                                tagId
                            );
                        },
                        [](const drogon::orm::DrogonDbException&) {},
                        tagName
                    );
                } else {
                    // 使用现有标签
                    int tagId = result[0]["id"].as<int>();
                    
                    // 创建文章标签关联
                    dbManager.executeQuery(
                        "INSERT INTO article_tags (article_id, tag_id) VALUES ($1, $2)",
                        [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {},
                        articleId,
                        tagId
                    );
                }
            },
            [](const drogon::orm::DrogonDbException&) {},
            tagName
        );
    }
} 