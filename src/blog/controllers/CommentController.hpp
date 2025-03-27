#pragma once

#include <drogon/HttpController.h>
#include <functional>
#include <string>
#include <json/json.h>

// 添加前向声明
class DbManager;

namespace api {
namespace v1 {

/**
 * 评论控制器
 * 处理评论的添加、获取等操作
 */
class CommentController : public drogon::HttpController<CommentController> {
public:
    /**
     * 构造函数
     */
    CommentController() = default;
    
    /**
     * 方法注册
     */
    METHOD_LIST_BEGIN
    // 获取文章评论列表
    ADD_METHOD_TO(CommentController::getArticleComments, "/api/articles/{article_id}/comments", drogon::Get);
    // 添加评论
    ADD_METHOD_TO(CommentController::addComment, "/api/comments", drogon::Post, "JwtAuthFilter");
    // 添加匿名评论
    ADD_METHOD_TO(CommentController::addAnonymousComment, "/api/comments/anonymous", drogon::Post);
    // 回复评论
    ADD_METHOD_TO(CommentController::replyComment, "/api/comments/{id}/reply", drogon::Post, "JwtAuthFilter");
    // 删除评论
    ADD_METHOD_TO(CommentController::deleteComment, "/api/comments/{id}", drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END
    
    /**
     * 获取文章评论列表
     */
    void getArticleComments(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          int article_id) const;
    
    /**
     * 添加评论
     */
    void addComment(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    
    /**
     * 添加匿名评论
     */
    void addAnonymousComment(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    
    /**
     * 回复评论
     */
    void replyComment(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    int id) const;
    
    /**
     * 删除评论
     */
    void deleteComment(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     int id) const;

private:
    /**
     * 验证评论数据
     */
    bool validateCommentData(const Json::Value& data, std::string& error) const;
    
    /**
     * 插入评论（辅助方法）
     */
    void insertComment(DbManager& dbManager, int articleId, const std::string& userUuid, 
                    const std::string& content, int parentId,
                    const std::string& authorName, const std::string& authorEmail,
                    const std::function<void(const drogon::HttpResponsePtr&)>& callback) const;
};

} // namespace v1
} // namespace api 