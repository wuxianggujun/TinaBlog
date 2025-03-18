//
// Created by wuxianggujun on 2025/3/14.
//

#include "BlogRouter.hpp"
#include <BlogConfig.hpp>
#include "BlogModule.hpp"
#include "BlogPostManager.hpp"
#include <fstream>
#include <sstream>
#include "db/BlogPostDao.hpp"
#include <algorithm>
#include <cstring>
#include "JsonResponse.hpp"

// 静态函数声明废弃，移动到BlogModule.cpp中实现
// 将其他函数声明为extern，使其可在其他文件中使用
extern ngx_int_t handleBlogIndex(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleBlogPost(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleBlogCategory(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleBlogTag(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleAdmin(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleAddPost(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleEditPost(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleDeletePost(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleBlogRedirect(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleGetPostForEdit(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleAdminStats(ngx_http_request_t* r, const RouteParams& params);
extern ngx_int_t handleOptionsRequest(ngx_http_request_t* r, const RouteParams& params);

// 发送JSON响应的向后兼容函数 - 将调用转发给JsonResponse
ngx_int_t sendJsonResponse(ngx_http_request_t* r, const std::string& jsonContent, ngx_uint_t status) {
    return JsonResponse::send(r, jsonContent, status);
}

// 重载sendJsonResponse函数,直接接收json对象 - 将调用转发给JsonResponse
ngx_int_t sendJsonResponse(ngx_http_request_t* r, const json& jsonObj, ngx_uint_t status) {
    return JsonResponse::send(r, jsonObj, status);
}

// 处理博客首页
ngx_int_t handleBlogIndex(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 使用NgxLog记录日志
        NgxLog logger(r);
        logger.info("处理博客首页API请求");
        
        // 从数据库中获取文章列表
        BlogPostDao dao;
        std::vector<BlogPostRecord> posts = dao.getAllPosts(10); // 获取前10篇文章
        
        // 使用nlohmann/json库构建响应
        json response;
        response["success"] = true;
        response["data"]["posts"] = json::array();
        
        for (const auto& post : posts) {
            json postJson;
            postJson["id"] = post.id;
            postJson["title"] = post.title;
            postJson["author"] = post.author;
            postJson["summary"] = post.summary;
            postJson["created_at"] = post.created_at;
            postJson["updated_at"] = post.updated_at;
            postJson["view_count"] = post.view_count;
            
            // 添加分类
            postJson["categories"] = post.categories;
            
            // 添加标签
            postJson["tags"] = post.tags;
            
            // 将文章添加到列表
            response["data"]["posts"].push_back(postJson);
        }
        
        // 发送JSON响应
        return sendJsonResponse(r, response);
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理博客首页API异常: %s", e.what());
        
        // 构建错误响应
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 辅助函数：转义JSON字符串
std::string escapejson(const std::string& input) {
    std::string output;
    output.reserve(input.length());
    
    for (size_t i = 0; i < input.length(); i++) {
        char ch = input[i];
        switch (ch) {
            case '\"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (ch < 32) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    output += buf;
                } else {
                    output += ch;
                }
        }
    }
    
    return output;
}

// 处理博客文章详情页
ngx_int_t handleBlogPost(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求和日志
        NgxRequest request(r);
        NgxLog logger(r);
        logger.info("处理博客文章详情API请求");

        // 从路由参数中获取文章ID
        std::string postIdParam = params.at("id");
        int postId = std::stoi(postIdParam);

        // 创建配置对象
        BlogConfig config(request);
        
        // 从数据库获取文章
        BlogPostDao dao;
        auto post = dao.getPostById(postId);
        
        // 使用nlohmann/json库构建响应
        json response;
        
        if (post.has_value()) {
            // 更新文章浏览量
            dao.incrementViewCount(postId);
            
            response["success"] = true;
            response["data"]["id"] = post->id;
            response["data"]["title"] = post->title;
            response["data"]["content"] = post->content;
            response["data"]["summary"] = post->summary;
            response["data"]["author"] = post->author;
            response["data"]["created_at"] = post->created_at;
            response["data"]["updated_at"] = post->updated_at;
            response["data"]["view_count"] = post->view_count + 1; // 包括当前访问
            
            // 添加分类和标签
            response["data"]["categories"] = post->categories;
            response["data"]["tags"] = post->tags;
        } else {
            // 文章不存在
            logger.warn("Post with ID %d not found", postId);
            response["success"] = false;
            response["error"]["code"] = 404;
            response["error"]["message"] = "文章不存在";
            
            return sendJsonResponse(r, response, NGX_HTTP_NOT_FOUND);
        }
        
        // 发送JSON响应
        return sendJsonResponse(r, response);
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理博客文章详情API异常: %s", e.what());
        
        // 构建错误响应
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 处理博客分类页面
ngx_int_t handleBlogCategory(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 使用NgxLog记录日志
        NgxLog logger(r);
        
        // 获取分类参数
        auto it = params.find("name");
        if (it == params.end()) {
            logger.error("缺少分类参数");
            
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"]["code"] = 400;
            errorResponse["error"]["message"] = "缺少分类参数";
            
            return sendJsonResponse(r, errorResponse, NGX_HTTP_BAD_REQUEST);
        }
        
        std::string category = it->second;
        logger.info("处理分类API请求: %s", category.c_str());
        
        // 从数据库获取该分类的文章
        BlogPostDao dao;
        auto posts = dao.getPostsByCategory(category, 10); // 获取前10篇文章
        
        // 使用nlohmann/json库构建响应
        json response;
        response["success"] = true;
        response["data"]["category"] = category;
        response["data"]["posts"] = json::array();
        
        for (const auto& post : posts) {
            json postJson;
            postJson["id"] = post.id;
            postJson["title"] = post.title;
            postJson["author"] = post.author;
            postJson["summary"] = post.summary;
            postJson["created_at"] = post.created_at;
            postJson["updated_at"] = post.updated_at;
            postJson["view_count"] = post.view_count;
            
            // 添加分类和标签
            postJson["categories"] = post.categories;
            postJson["tags"] = post.tags;
            
            // 将文章添加到列表
            response["data"]["posts"].push_back(postJson);
        }
        
        // 发送JSON响应
        return sendJsonResponse(r, response);
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理分类API请求异常: %s", e.what());
        
        // 构建错误响应
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 处理博客标签页面
ngx_int_t handleBlogTag(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 使用NgxLog记录日志
        NgxLog logger(r);
        
        // 获取标签参数
        auto it = params.find("name");
        if (it == params.end()) {
            logger.error("缺少标签参数");
            
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"]["code"] = 400;
            errorResponse["error"]["message"] = "缺少标签参数";
            
            return sendJsonResponse(r, errorResponse, NGX_HTTP_BAD_REQUEST);
        }
        
        std::string tag = it->second;
        logger.info("处理标签API请求: %s", tag.c_str());
        
        // 从数据库获取该标签的文章
        BlogPostDao dao;
        auto posts = dao.getPostsByTag(tag, 10); // 获取前10篇文章
        
        // 使用nlohmann/json库构建响应
        json response;
        response["success"] = true;
        response["data"]["tag"] = tag;
        response["data"]["posts"] = json::array();
        
        for (const auto& post : posts) {
            json postJson;
            postJson["id"] = post.id;
            postJson["title"] = post.title;
            postJson["author"] = post.author;
            postJson["summary"] = post.summary;
            postJson["created_at"] = post.created_at;
            postJson["updated_at"] = post.updated_at;
            postJson["view_count"] = post.view_count;
            
            // 添加分类和标签
            postJson["categories"] = post.categories;
            postJson["tags"] = post.tags;
            
            // 将文章添加到列表
            response["data"]["posts"].push_back(postJson);
        }
        
        // 发送JSON响应
        return sendJsonResponse(r, response);
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理标签API请求异常: %s", e.what());
        
        // 构建错误响应
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 处理博客管理页面
ngx_int_t handleAdmin(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 创建日志对象
        NgxLog logger(r);
        logger.info("处理博客管理API请求");
        
        // 从数据库获取所有文章
        BlogPostDao dao;
        auto posts = dao.getAllPosts();
        
        // 使用nlohmann/json库构建响应
        json response;
        response["success"] = true;
        response["data"]["posts"] = json::array();
        
        for (const auto& post : posts) {
            json postJson;
            postJson["id"] = post.id;
            postJson["title"] = post.title;
            postJson["author"] = post.author;
            postJson["summary"] = post.summary;
            postJson["created_at"] = post.created_at;
            postJson["updated_at"] = post.updated_at;
            postJson["view_count"] = post.view_count;
            
            // 添加分类和标签
            postJson["categories"] = post.categories;
            postJson["tags"] = post.tags;
            
            // 将文章添加到列表
            response["data"]["posts"].push_back(postJson);
        }
        
        // 发送JSON响应
        return sendJsonResponse(r, response);
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理博客管理API异常: %s", e.what());
        
        // 构建错误响应
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 处理添加文章请求
ngx_int_t handleAddPost(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        NgxLog logger(r);
        
        // 如果是GET请求，返回错误，因为API只支持POST
        if (request.getMethod() != NGX_HTTP_POST) {
            logger.warn("Add post API only supports POST requests");
            
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"]["code"] = 405;
            errorResponse["error"]["message"] = "Method Not Allowed. Use POST.";
            
            return sendJsonResponse(r, errorResponse, NGX_HTTP_NOT_ALLOWED);
        }
        
        // 处理POST请求：新增文章
        logger.info("处理添加文章POST请求");
        
        // 读取POST数据
        std::string postData = request.getRequestBody();
        
        // 解析表单数据
        std::unordered_map<std::string, std::string> formData = request.parseFormData(postData);
        
        // 提取表单字段
        std::string title = formData["title"];
        std::string content = formData["content"];
        std::string summary = formData["summary"];
        std::string author = formData["author"];
        std::string categoriesStr = formData["categories"];
        std::string tagsStr = formData["tags"];
        bool isPublished = formData.find("publish") != formData.end() && formData["publish"] == "on";
        
        // 检查必填字段
        if (title.empty() || content.empty()) {
            logger.warn("Title or content is empty");
            
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"]["code"] = 400;
            errorResponse["error"]["message"] = "标题和内容不能为空";
            errorResponse["error"]["fields"]["title"] = title.empty() ? "缺失" : "有效";
            errorResponse["error"]["fields"]["content"] = content.empty() ? "缺失" : "有效";
            
            return sendJsonResponse(r, errorResponse, NGX_HTTP_BAD_REQUEST);
        }
        
        // 处理分类（以逗号分隔）
        std::vector<std::string> categories;
        if (!categoriesStr.empty()) {
            std::stringstream ss(categoriesStr);
            std::string category;
            while (std::getline(ss, category, ',')) {
                // 去除前后空格
                category.erase(0, category.find_first_not_of(" \t\n\r\f\v"));
                category.erase(category.find_last_not_of(" \t\n\r\f\v") + 1);
                if (!category.empty()) {
                    categories.push_back(category);
                }
            }
        }
        
        // 处理标签（以逗号分隔）
        std::vector<std::string> tags;
        if (!tagsStr.empty()) {
            std::stringstream ss(tagsStr);
            std::string tag;
            while (std::getline(ss, tag, ',')) {
                // 去除前后空格
                tag.erase(0, tag.find_first_not_of(" \t\n\r\f\v"));
                tag.erase(tag.find_last_not_of(" \t\n\r\f\v") + 1);
                if (!tag.empty()) {
                    tags.push_back(tag);
                }
            }
        }
        
        // 使用DAO保存文章到数据库
        BlogPostDao dao;
        int postId = dao.createPost(title, content, summary, author,categories, tags, isPublished);
        
        if (postId > 0) {
            logger.info("文章创建成功，ID: %d", postId);
            
            // 使用nlohmann/json库构建成功响应
            json response;
            response["success"] = true;
            response["data"]["id"] = postId;
            response["data"]["title"] = title;
            response["data"]["message"] = "文章创建成功";
            
            return sendJsonResponse(r, response);
        }
        logger.error("创建文章失败");
            
        // 使用nlohmann/json库构建错误响应
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = "保存文章失败，请重试！";
            
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理添加文章请求异常: %s", e.what());
        
        // 使用nlohmann/json库构建错误响应
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 处理文章编辑页面
ngx_int_t handleEditPost(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        NgxLog logger(r);
        
        // 获取文章ID
        std::string postIdParam = params.at("id");
        int postId = std::stoi(postIdParam);
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 如果是GET请求，返回文章信息
        if (request.getMethod() == NGX_HTTP_GET) {
            logger.info("获取文章编辑信息, ID: %d", postId);
            
            // 从数据库获取文章
            BlogPostDao dao;
            auto post = dao.getPostById(postId);
            
            if (!post.has_value()) {
                logger.warn("Post with ID %d not found", postId);
                
                json errorResponse;
                errorResponse["success"] = false;
                errorResponse["error"]["code"] = 404;
                errorResponse["error"]["message"] = "文章不存在";
                
                return sendJsonResponse(r, errorResponse, NGX_HTTP_NOT_FOUND);
            }
            
            // 使用nlohmann/json库构建响应
            json response;
            response["success"] = true;
            response["data"]["id"] = post->id;
            response["data"]["title"] = post->title;
            response["data"]["content"] = post->content;
            response["data"]["summary"] = post->summary;
            response["data"]["author"] = post->author;
            response["data"]["categories"] = post->categories;
            response["data"]["tags"] = post->tags;
            
            return sendJsonResponse(r, response);
        }
        // 处理POST请求：更新文章
        else if (request.getMethod() == NGX_HTTP_POST) {
            logger.info("处理文章更新请求, ID: %d", postId);
            
            // 读取POST数据
            std::string postData = request.getRequestBody();
            
            // 解析表单数据
            std::unordered_map<std::string, std::string> formData = request.parseFormData(postData);
            
            // 提取表单字段
            std::string title = formData["title"];
            std::string content = formData["content"];
            std::string summary = formData["summary"];
            std::string categoriesStr = formData["categories"];
            std::string tagsStr = formData["tags"];
            bool isPublished = formData.find("publish") != formData.end() && formData["publish"] == "on";
            
            // 检查必填字段
            if (title.empty() || content.empty()) {
                logger.warn("Title or content is empty");
                
                json errorResponse;
                errorResponse["success"] = false;
                errorResponse["error"]["code"] = 400;
                errorResponse["error"]["message"] = "标题和内容不能为空";
                
                return sendJsonResponse(r, errorResponse, NGX_HTTP_BAD_REQUEST);
            }
            
            // 处理分类
            std::vector<std::string> categories;
            if (!categoriesStr.empty()) {
                std::stringstream ss(categoriesStr);
                std::string category;
                while (std::getline(ss, category, ',')) {
                    category.erase(0, category.find_first_not_of(" \t\n\r\f\v"));
                    category.erase(category.find_last_not_of(" \t\n\r\f\v") + 1);
                    if (!category.empty()) {
                        categories.push_back(category);
                    }
                }
            }
            
            // 处理标签
            std::vector<std::string> tags;
            if (!tagsStr.empty()) {
                std::stringstream ss(tagsStr);
                std::string tag;
                while (std::getline(ss, tag, ',')) {
                    tag.erase(0, tag.find_first_not_of(" \t\n\r\f\v"));
                    tag.erase(tag.find_last_not_of(" \t\n\r\f\v") + 1);
                    if (!tag.empty()) {
                        tags.push_back(tag);
                    }
                }
            }
            
            // 更新文章 - 使用正确的参数列表
            BlogPostDao dao;
            bool updateSuccess = dao.updatePost(postId, title, content, summary, isPublished);
            
            // 单独设置分类和标签
            bool categoriesSuccess = true;
            bool tagsSuccess = true;
            
            if (updateSuccess) {
                // 设置分类
                categoriesSuccess = dao.setPostCategories(postId, categories);
                
                // 设置标签
                tagsSuccess = dao.setPostTags(postId, tags);
            }
            
            bool success = updateSuccess && categoriesSuccess && tagsSuccess;
            
            if (success) {
                logger.info("文章更新成功, ID: %d", postId);
                
                json response;
                response["success"] = true;
                response["data"]["id"] = postId;
                response["data"]["message"] = "文章更新成功";
                
                return sendJsonResponse(r, response);
            } else {
                logger.error("文章更新失败, ID: %d", postId);
                
                json errorResponse;
                errorResponse["success"] = false;
                errorResponse["error"]["code"] = 500;
                errorResponse["error"]["message"] = updateSuccess ? 
                    (categoriesSuccess ? "设置标签失败" : "设置分类失败") : 
                    "更新文章基本信息失败";
                
                return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
            }
        }
        // 其他HTTP方法
        else {
            logger.warn("Edit post API does not support current request method: %d", request.getMethod());
            
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"]["code"] = 405;
            errorResponse["error"]["message"] = "Method Not Allowed. Use GET or POST.";
            
            return sendJsonResponse(r, errorResponse, NGX_HTTP_NOT_ALLOWED);
        }
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理文章编辑请求异常: %s", e.what());
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 处理文章删除功能
ngx_int_t handleDeletePost(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求
        NgxRequest request(r);
        NgxLog logger(r);
        
        // 获取文章ID
        std::string postIdParam = params.at("id");
        int postId = std::stoi(postIdParam);
        
        logger.info("处理文章删除请求, ID: %d", postId);
        
        // 验证请求方法
        if (request.getMethod() != NGX_HTTP_POST && request.getMethod() != NGX_HTTP_DELETE) {
            logger.warn("Delete post API only supports POST or DELETE requests");
            
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"]["code"] = 405;
            errorResponse["error"]["message"] = "Method Not Allowed. Use POST or DELETE.";
            
            return sendJsonResponse(r, errorResponse, NGX_HTTP_NOT_ALLOWED);
        }
        
        // 删除文章
        BlogPostDao dao;
        bool success = dao.deletePost(postId);
        
        if (success) {
            logger.info("文章删除成功, ID: %d", postId);
            
            json response;
            response["success"] = true;
            response["data"]["id"] = postId;
            response["data"]["message"] = "文章删除成功";
            
            return sendJsonResponse(r, response);
        } else {
            logger.error("文章删除失败, ID: %d", postId);
            
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"]["code"] = 404;
            errorResponse["error"]["message"] = "文章不存在或删除失败";
            
            return sendJsonResponse(r, errorResponse, NGX_HTTP_NOT_FOUND);
        }
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理文章删除请求异常: %s", e.what());
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 处理OPTIONS请求，支持CORS预检请求
ngx_int_t handleOptionsRequest(ngx_http_request_t* r, const RouteParams& params) {
    NgxLog logger(r);
    logger.info("处理OPTIONS请求: %s", ((NgxRequest(r)).getUri()).c_str());
    
    // 设置响应头部
    r->headers_out.status = NGX_HTTP_OK;
    
    // 设置CORS头，允许所有来源访问
    ngx_table_elt_t *h = static_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    h->hash = 1;
    h->key.len = sizeof("Access-Control-Allow-Origin") - 1;
    h->key.data = (u_char *) "Access-Control-Allow-Origin";
    h->value.len = sizeof("*") - 1;
    h->value.data = (u_char *) "*";
    
    // 添加允许的方法
    h = static_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    h->hash = 1;
    h->key.len = sizeof("Access-Control-Allow-Methods") - 1;
    h->key.data = (u_char *) "Access-Control-Allow-Methods";
    h->value.len = sizeof("GET, POST, PUT, DELETE, OPTIONS") - 1;
    h->value.data = (u_char *) "GET, POST, PUT, DELETE, OPTIONS";
    
    // 添加允许的头部
    h = static_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    h->hash = 1;
    h->key.len = sizeof("Access-Control-Allow-Headers") - 1;
    h->key.data = (u_char *) "Access-Control-Allow-Headers";
    h->value.len = sizeof("Content-Type, Authorization, X-Requested-With") - 1;
    h->value.data = (u_char *) "Content-Type, Authorization, X-Requested-With";
    
    // 允许凭证
    h = static_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    h->hash = 1;
    h->key.len = sizeof("Access-Control-Allow-Credentials") - 1;
    h->key.data = (u_char *) "Access-Control-Allow-Credentials";
    h->value.len = sizeof("true") - 1;
    h->value.data = (u_char *) "true";
    
    // 设置最大缓存时间
    h = static_cast<ngx_table_elt_t*>(ngx_list_push(&r->headers_out.headers));
    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    h->hash = 1;
    h->key.len = sizeof("Access-Control-Max-Age") - 1;
    h->key.data = (u_char *) "Access-Control-Max-Age";
    h->value.len = sizeof("86400") - 1; // 24小时
    h->value.data = (u_char *) "86400";
    
    // 设置响应长度为0
    r->headers_out.content_length_n = 0;
    
    // 发送HTTP头
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK) {
        return rc;
    }
    
    // 创建一个空的缓冲区表示响应体结束
    ngx_buf_t *b = static_cast<ngx_buf_t*>(ngx_pcalloc(r->pool, sizeof(ngx_buf_t)));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 标记为最后一个缓冲区，但不包含数据
    b->last_buf = 1;      // 标记为最后一个缓冲区
    b->last_in_chain = 1; // 标记为链中最后一个
    b->pos = b->last = nullptr; // 明确指示这是一个零大小的缓冲区
    b->sync = 1;          // 标记为同步操作
    
    ngx_chain_t out;
    out.buf = b;
    out.next = nullptr;
    
    return ngx_http_output_filter(r, &out);
}

// 处理获取文章编辑数据的请求
ngx_int_t handleGetPostForEdit(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求和日志
        NgxRequest request(r);
        NgxLog logger(r);
        logger.info("处理获取文章编辑数据API请求");

        // 从路由参数中获取文章ID
        std::string postIdParam = params.at("id");
        int postId = std::stoi(postIdParam);

        // 创建配置对象
        BlogConfig config(request);
        
        // 从数据库获取文章
        BlogPostDao dao;
        auto post = dao.getPostById(postId);
        
        // 使用nlohmann/json库构建响应
        json response;
        
        if (post.has_value()) {
            response["success"] = true;
            response["data"]["id"] = post->id;
            response["data"]["title"] = post->title;
            response["data"]["content"] = post->content;
            response["data"]["summary"] = post->summary;
            response["data"]["author"] = post->author;
            response["data"]["created_at"] = post->created_at;
            response["data"]["updated_at"] = post->updated_at;
            response["data"]["published"] = post->published;
            
            // 添加分类和标签
            response["data"]["categories"] = post->categories;
            response["data"]["tags"] = post->tags;
        } else {
            // 文章不存在
            logger.warn("Post with ID %d not found", postId);
            response["success"] = false;
            response["error"]["code"] = 404;
            response["error"]["message"] = "文章不存在";
            
            return sendJsonResponse(r, response, NGX_HTTP_NOT_FOUND);
        }
        
        // 发送JSON响应
        return sendJsonResponse(r, response);
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理获取文章编辑数据请求异常: %s", e.what());
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}

// 处理管理面板统计数据API请求
ngx_int_t handleAdminStats(ngx_http_request_t* r, const RouteParams& params) {
    try {
        // 封装请求和日志
        NgxRequest request(r);
        NgxLog logger(r);
        logger.info("处理管理面板统计数据API请求");
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 从数据库获取统计数据
        BlogPostDao dao;
        
        // 获取总文章数
        int totalPosts = dao.getPostCount();
        
        // 获取已发布文章数
        int publishedPosts = dao.getPublishedPostCount();
        
        // 获取总浏览量
        int totalViews = dao.getTotalViewCount();
        
        // 获取分类数量
        int categoriesCount = dao.getCategoryCount();
        
        // 获取标签数量
        int tagsCount = dao.getTagCount();
        
        // 获取最近文章
        std::vector<BlogPostRecord> recentPosts = dao.getAllPosts(5); // 获取5篇最新文章
        
        // 构建响应
        json response;
        response["success"] = true;
        response["data"]["stats"]["total_posts"] = totalPosts;
        response["data"]["stats"]["published_posts"] = publishedPosts;
        response["data"]["stats"]["total_views"] = totalViews;
        response["data"]["stats"]["categories_count"] = categoriesCount;
        response["data"]["stats"]["tags_count"] = tagsCount;
        
        // 添加最近文章
        response["data"]["recent_posts"] = json::array();
        for (const auto& post : recentPosts) {
            json postJson;
            postJson["id"] = post.id;
            postJson["title"] = post.title;
            postJson["author"] = post.author;
            postJson["created_at"] = post.created_at;
            postJson["view_count"] = post.view_count;
            
            response["data"]["recent_posts"].push_back(postJson);
        }
        
        // 发送JSON响应
        return sendJsonResponse(r, response);
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                     "处理管理面板统计数据请求异常: %s", e.what());
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"]["code"] = 500;
        errorResponse["error"]["message"] = e.what();
        
        return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
}
