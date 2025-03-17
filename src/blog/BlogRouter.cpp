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
#include <regex>
#include <nlohmann/json.hpp>

// 使用nlohmann/json命名空间
using json = nlohmann::json;

// Route::parsePattern 实现
void Route::parsePattern() {
    std::string current;
    bool inParam = false;

    for (char c : pattern) {
        if (c == ':' && !inParam) {
            inParam = true;
            current.clear();
        }
        else if (inParam && (c == '/' || c == '.')) {
            if (!current.empty()) {
                paramNames.push_back(std::move(current));
            }
            inParam = false;
        }
        else if (inParam) {
            current += c;
        }
    }

    if (inParam && !current.empty()) {
        paramNames.push_back(std::move(current));
    }
}

// Route::match 实现
bool Route::match(const ngx_str_t& uri, RouteParams& params) const {
    std::string uriStr(reinterpret_cast<const char*>(uri.data), uri.len);
    
    // 打印调试信息
    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
        "尝试匹配: URI=%s 与模式=%s", 
        uriStr.c_str(), pattern.c_str());
    
    // 特殊处理根路径和/blog路径
    if (pattern == "/" && (uriStr == "/" || uriStr.empty())) {
        return true;
    }
    
    if (pattern == "/blog" && (uriStr == "/blog" || uriStr == "/blog/")) {
        return true;
    }

    // 如果模式不包含参数，直接比较
    if (paramNames.empty()) {
        // 完全相同的情况
        if (uriStr == pattern) {
            ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                "完全匹配成功: URI=%s 与模式=%s", 
                uriStr.c_str(), pattern.c_str());
            return true;
        }
        
        // 尾部斜杠的特殊处理
        if (pattern.back() == '/' && uriStr == pattern.substr(0, pattern.length()-1)) {
            ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                "尾部斜杠匹配成功: URI=%s 与模式=%s", 
                uriStr.c_str(), pattern.c_str());
            return true;
        }
        
        if (uriStr.back() == '/' && pattern == uriStr.substr(0, uriStr.length()-1)) {
            ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                "尾部斜杠匹配成功: URI=%s 与模式=%s", 
                uriStr.c_str(), pattern.c_str());
            return true;
        }
        
        ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
            "匹配失败: URI=%s 与模式=%s", 
            uriStr.c_str(), pattern.c_str());
        return false;
    }

    // 将模式转换为正则表达式进行匹配
    // 简化实现：仅支持基本的 /:param/ 格式
    size_t patternPos = 0;
    size_t uriPos = 0;

    for (const auto& paramName : paramNames) {
        // 找到参数前的静态部分
        size_t paramPos = pattern.find(":" + paramName, patternPos);
        if (paramPos == std::string::npos) continue;

        // 检查静态部分是否匹配
        std::string staticPart = pattern.substr(patternPos, paramPos - patternPos);
        if (uriStr.substr(uriPos, staticPart.length()) != staticPart) {
            ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                "静态部分不匹配: URI=%s 部分=%s", 
                uriStr.substr(uriPos, staticPart.length()).c_str(), staticPart.c_str());
            return false;
        }

        // 移动位置
        uriPos += staticPart.length();
        patternPos = paramPos + paramName.length() + 1; // +1 for ':'

        // 寻找参数结束位置
        size_t paramEndPos;
        if (patternPos >= pattern.length()) {
            // 参数在结尾
            paramEndPos = uriStr.length();
        }
        else {
            // 参数后面有内容，查找下一个分隔符
            char nextChar = pattern[patternPos];
            paramEndPos = uriStr.find(nextChar, uriPos);
            if (paramEndPos == std::string::npos) {
                ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
                    "找不到参数结束位置: 字符=%c URI=%s", 
                    nextChar, uriStr.c_str());
                return false;
            }
        }

        // 提取参数值
        std::string paramValue = uriStr.substr(uriPos, paramEndPos - uriPos);
        params[paramName] = paramValue;
        
        ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
            "提取参数: %s=%s", 
            paramName.c_str(), paramValue.c_str());

        // 更新位置
        uriPos = paramEndPos;
    }

    // 检查路径末尾
    std::string endPart = pattern.substr(patternPos);
    bool result = uriStr.substr(uriPos) == endPart;
    
    if (result) {
        ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
            "完整匹配成功: 剩余URI=%s 剩余模式=%s", 
            uriStr.substr(uriPos).c_str(), endPart.c_str());
    } else {
        ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, 
            "末尾不匹配: 剩余URI=%s 剩余模式=%s", 
            uriStr.substr(uriPos).c_str(), endPart.c_str());
    }
    
    return result;
}

// Router类实现
void Router::addRoute(const Route& route) {
    routes.push_back(route);
}

void Router::reset() {
    routes.clear();
}

ngx_int_t Router::route(ngx_http_request_t* r) {
    // 获取请求URI
    std::string uri((char*)r->uri.data, r->uri.len);
    
    NgxLog logger(r);
    logger.info("正在路由请求: %s, 方法: %d", uri.c_str(), r->method);
    logger.info("当前路由器有 %d 条路由规则", routes.size());
    
    // 提取参数
    RouteParams params;
    
    // 遍历所有路由，寻找匹配
    for (const auto& route : routes) {
        // 检查HTTP方法匹配
        if (route.method != ANY_METHOD && route.method != r->method) {
            logger.debug("路由 '%s' 的HTTP方法不匹配: %d != %d", 
                      route.pattern.c_str(), route.method, r->method);
            continue;
        }
        
        // 检查URI匹配
        if (route.match(r->uri, params)) {
            logger.info("找到匹配的路由: %s，参数数量: %d", 
                      route.pattern.c_str(), params.size());
            
            // 查看提取的参数
            for (const auto& param : params) {
                logger.debug("参数: %s = %s", param.first.c_str(), param.second.c_str());
            }
            
            // 调用处理器
            return route.handler(r, params);
        }
    }
    
    logger.warn("没有找到匹配的路由，共尝试 %d 条路由规则", routes.size());
    return NGX_DECLINED;
}

size_t Router::getRouteCount() const {
    return routes.size();
}

std::vector<std::string> Router::dumpRoutes() const {
    std::vector<std::string> result;
    
    for (const auto& route : routes) {
        std::string methodStr;
        switch (route.method) {
            case GET_METHOD: methodStr = "GET"; break;
            case POST_METHOD: methodStr = "POST"; break;
            case PUT_METHOD: methodStr = "PUT"; break;
            case DELETE_METHOD: methodStr = "DELETE"; break;
            case HEAD_METHOD: methodStr = "HEAD"; break;
            case ANY_METHOD: methodStr = "ANY"; break;
            default: methodStr = "UNKNOWN"; break;
        }
        
        result.push_back(methodStr + " " + route.pattern);
    }
    
    return result;
}

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

// 辅助函数：发送JSON响应
ngx_int_t sendJsonResponse(ngx_http_request_t* r, const std::string& jsonContent, ngx_uint_t status = NGX_HTTP_OK) {
    // 设置响应头部
    r->headers_out.status = status;
    r->headers_out.content_type.len = sizeof("application/json") - 1;
    r->headers_out.content_type.data = (u_char *) "application/json";
    
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
    
    // 设置响应长度
    r->headers_out.content_length_n = jsonContent.length();
    
    // 发送HTTP头
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    
    // 创建缓冲区
    ngx_buf_t *b = static_cast<ngx_buf_t*>(ngx_pcalloc(r->pool, sizeof(ngx_buf_t)));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    // 复制数据到缓冲区
    u_char *data = static_cast<u_char*>(ngx_palloc(r->pool, jsonContent.length()));
    if (data == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    ngx_memcpy(data, jsonContent.c_str(), jsonContent.length());
    
    // 设置缓冲区
    b->pos = data;
    b->last = data + jsonContent.length();
    b->memory = 1;    // 内存缓冲区
    b->last_buf = 1;  // 最后一个缓冲区
    
    // 创建响应链
    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;
    
    // 发送响应体
    return ngx_http_output_filter(r, &out);
}

// 重载sendJsonResponse函数,直接接收json对象
ngx_int_t sendJsonResponse(ngx_http_request_t* r, const json& jsonObj, ngx_uint_t status = NGX_HTTP_OK) {
    // 将json对象转换为字符串并调用原来的函数
    std::string jsonContent = jsonObj.dump(2); // 缩进2个空格
    return sendJsonResponse(r, jsonContent, status);
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
            logger.warn("未找到ID为 %d 的文章", postId);
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
        
        // 创建配置对象
        BlogConfig config(request);
        
        // 如果是GET请求，返回错误，因为API只支持POST
        if (request.getMethod() != NGX_HTTP_POST) {
            logger.warn("添加文章API只支持POST请求");
            
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
        std::string categoriesStr = formData["categories"];
        std::string tagsStr = formData["tags"];
        bool isPublished = formData.find("publish") != formData.end() && formData["publish"] == "on";
        
        // 检查必填字段
        if (title.empty() || content.empty()) {
            logger.warn("提交的标题或内容为空");
            
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
        int postId = dao.createPost(title, content, summary, categories, tags, isPublished);
        
        if (postId > 0) {
            logger.info("文章创建成功，ID: %d", postId);
            
            // 使用nlohmann/json库构建成功响应
            json response;
            response["success"] = true;
            response["data"]["id"] = postId;
            response["data"]["title"] = title;
            response["data"]["message"] = "文章创建成功";
            
            return sendJsonResponse(r, response);
        } else {
            logger.error("创建文章失败");
            
            // 使用nlohmann/json库构建错误响应
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"]["code"] = 500;
            errorResponse["error"]["message"] = "保存文章失败，请重试！";
            
            return sendJsonResponse(r, errorResponse, NGX_HTTP_INTERNAL_SERVER_ERROR);
        }
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
                logger.warn("未找到ID为 %d 的文章", postId);
                
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
                logger.warn("提交的标题或内容为空");
                
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
            logger.warn("编辑文章API不支持当前请求方法: %d", request.getMethod());
            
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
            logger.warn("删除文章API仅支持POST或DELETE请求");
            
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
