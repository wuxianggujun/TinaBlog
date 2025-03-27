#pragma once
#include <drogon/orm/Result.h>
#include <json/json.h>
#include <string>

namespace utils {

/**
 * 文章相关工具函数
 */
class ArticleUtils {
public:
    /**
     * 构建文章响应数据（分页）
     */
    static Json::Value buildArticleResponse(const drogon::orm::Result& result, int page, int pageSize);
    
    /**
     * 从文章内容生成摘要
     */
    static std::string generateSummary(const std::string& content, size_t maxLength = 150);
    
    /**
     * 从字符串生成slug
     */
    static std::string generateSlug(const std::string& text);
};

} // namespace utils 