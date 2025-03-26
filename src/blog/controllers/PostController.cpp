#include "PostController.hpp"
#include "blog/utils/HttpUtils.hpp"
#include "blog/utils/ErrorCode.hpp"
#include "blog/db/DbManager.hpp"
#include <regex>
#include <iostream>
#include <algorithm>
#include <cctype>

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
    // 对每个分类进行处理
    for (const auto& category : categories) {
        if (!category.isString()) continue;
        
        std::string categoryName = category.asString();
        if (categoryName.empty()) continue;
        
        try {
            // 从分类名生成slug - 替换空格为连字符，转小写
            std::string slug = categoryName;
            // 转为小写
            std::transform(slug.begin(), slug.end(), slug.begin(), 
                [](unsigned char c){ return std::tolower(c); });
            // 替换空格为连字符
            std::replace(slug.begin(), slug.end(), ' ', '-');
            // 移除非字母数字和连字符的字符
            slug.erase(std::remove_if(slug.begin(), slug.end(), 
                [](unsigned char c){ return !(std::isalnum(c) || c == '-'); }), slug.end());
            
            std::cout << "正在处理分类: " << categoryName << ", slug: " << slug << std::endl;
            
            // 首先查找分类是否已存在
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
                // 分类不存在，创建新分类
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
            // 从标签名生成slug - 替换空格为连字符，转小写
            std::string slug = tagName;
            // 转为小写
            std::transform(slug.begin(), slug.end(), slug.begin(), 
                [](unsigned char c){ return std::tolower(c); });
            // 替换空格为连字符
            std::replace(slug.begin(), slug.end(), ' ', '-');
            // 移除非字母数字和连字符的字符
            slug.erase(std::remove_if(slug.begin(), slug.end(), 
                [](unsigned char c){ return !(std::isalnum(c) || c == '-'); }), slug.end());
            
            std::cout << "正在处理标签: " << tagName << ", slug: " << slug << std::endl;
            
            // 首先查找标签是否已存在
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
                // 标签不存在，创建新标签
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
        }
        
        // 检查必要字段
        if (!jsonBody.isMember("title") || !jsonBody.isMember("content")) {
            // 回滚事务
            dbManager.executeQuery("ROLLBACK", [](const drogon::orm::Result&) {}, [](const drogon::orm::DrogonDbException&) {});
            callback(utils::createErrorResponse(utils::ErrorCode::INVALID_PARAMETER, "标题和内容不能为空"));
            return;
        }

        // 记录请求信息
        std::cout << "创建文章 - 请求字段:" << std::endl;
        std::cout << "  标题: " << jsonBody["title"].asString() << std::endl;
        std::cout << "  摘要: " << summary << std::endl;
        std::cout << "  Slug: " << slug << std::endl;
        std::cout << "  发布状态: " << (isPublished ? "是" : "否") << std::endl;
        
        // 记录分类信息
        std::cout << "  分类信息: ";
        if (jsonBody.isMember("categories")) {
            if (jsonBody["categories"].isArray()) {
                std::cout << "数组，长度 " << jsonBody["categories"].size() << std::endl;
            } else {
                std::cout << "非数组类型: " << jsonBody["categories"].type() << std::endl;
            }
        } else {
            std::cout << "字段不存在" << std::endl;
        }
        
        // 记录标签信息
        std::cout << "  标签信息: ";
        if (jsonBody.isMember("tags")) {
            if (jsonBody["tags"].isArray()) {
                std::cout << "数组，长度 " << jsonBody["tags"].size() << std::endl;
            } else {
                std::cout << "非数组类型: " << jsonBody["tags"].type() << std::endl;
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
                if (jsonBody.isMember("categories") && jsonBody["categories"].isArray() && !jsonBody["categories"].empty()) {
                    // 创建安全的回调函数
                    auto categoriesComplete = std::make_shared<std::function<void()>>([=, &dbManager]() {
                        // 分类处理完成，处理标签
                        if (jsonBody.isMember("tags") && jsonBody["tags"].isArray() && !jsonBody["tags"].empty()) {
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
                            handleTagsAsync(articleId, jsonBody["tags"], (*tagsComplete));
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
                    handleCategoriesAsync(articleId, jsonBody["categories"], (*categoriesComplete));
                } else if (jsonBody.isMember("tags") && jsonBody["tags"].isArray() && !jsonBody["tags"].empty()) {
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
                    handleTagsAsync(articleId, jsonBody["tags"], (*tagsComplete));
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
            jsonBody["title"].asString(),
            slug,
            jsonBody["content"].asString(),
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
 * 从名称生成slug
 */
std::string PostController::generateSlug(const std::string& name) const {
    std::string slug = name;
    // 转为小写
    std::transform(slug.begin(), slug.end(), slug.begin(), 
        [](unsigned char c){ return std::tolower(c); });
    // 替换空格为连字符
    std::replace(slug.begin(), slug.end(), ' ', '-');
    // 移除非字母数字和连字符的字符
    slug.erase(std::remove_if(slug.begin(), slug.end(), 
        [](unsigned char c){ return !(std::isalnum(c) || c == '-'); }), slug.end());
    // 确保没有连续的连字符
    slug = std::regex_replace(slug, std::regex("-+"), "-");
    // 去除首尾连字符
    slug = std::regex_replace(slug, std::regex("^-|-$"), "");
    
    return slug;
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
    
    *processNextCategory = [=, &dbManager, processNextCategory]() mutable {
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

        // 生成slug
        std::string slug = generateSlug(categoryName);
        std::cout << "异步处理分类: " << categoryName << ", slug: " << slug << std::endl;

        // 1. 异步检查分类是否存在
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
                    
                    // 可选：更新slug（可以并行进行，不影响主流程）
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
                    // 2b. 分类不存在，异步插入
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
    
    *processNextTag = [=, &dbManager, processNextTag]() mutable {
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

        // 生成slug
        std::string slug = generateSlug(tagName);
        std::cout << "异步处理标签: " << tagName << ", slug: " << slug << std::endl;

        // 创建本地副本以避免循环引用
        auto nextTagCallback = processNextTag;

        // 1. 异步检查标签是否存在
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
                    
                    // 可选：更新slug（可以并行进行，不影响主流程）
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
                    // 2b. 标签不存在，异步插入
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