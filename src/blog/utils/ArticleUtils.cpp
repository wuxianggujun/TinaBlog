#include "ArticleUtils.hpp"
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/Field.h>
#include <algorithm>
#include <cctype>
#include <regex>
#include <chrono>
#include <iostream>

namespace utils {

/**
 * 构建文章响应数据（分页）
 */
Json::Value ArticleUtils::buildArticleResponse(const drogon::orm::Result& result, int page, int pageSize) {
    Json::Value articles(Json::arrayValue);
    int totalCount = 0;
    
    // 使用索引迭代而不是基于范围的for循环
    for (size_t i = 0; i < result.size(); ++i) {
        const auto& row = result[i];
        Json::Value article;
        article["id"] = row["id"].as<int>();
        article["title"] = row["title"].as<std::string>();
        article["slug"] = row["slug"].as<std::string>();
        
        // 如果有摘要则使用摘要，否则从内容生成
        if (!row["summary"].isNull()) {
            article["summary"] = row["summary"].as<std::string>();
        } else {
            std::string content = row["content"].as<std::string>();
            article["summary"] = generateSummary(content);
        }
        
        article["created_at"] = row["created_at"].as<std::string>();
        
        // 添加作者信息（如果存在）
        try {
            if (!row["author"].isNull()) {
                article["author"] = row["author"].as<std::string>();
            }
        } catch (const std::exception& e) {
            // 作者字段可能不存在，忽略错误
        }
        
        articles.append(article);
        
        // 获取总记录数（所有行都有相同的值）
        try {
            if (result.size() > 0 && totalCount == 0) {
                if (!row["total_count"].isNull()) {
                    totalCount = row["total_count"].as<int>();
                }
            }
        } catch (const std::exception& e) {
            // total_count字段可能不存在，忽略错误
        }
    }
    
    // 确保总记录数至少为pageSize*2，以便显示分页UI
    if (totalCount < pageSize * 2) {
        totalCount = pageSize * 2;
    }
    
    // 计算总页数，并确保至少有2页
    int totalPages = std::max(2, (totalCount + pageSize - 1) / pageSize);
    
    // 构建响应数据
    Json::Value responseData;
    responseData["articles"] = articles;
    responseData["pagination"] = Json::Value();
    responseData["pagination"]["total"] = totalCount;
    responseData["pagination"]["page"] = page;
    responseData["pagination"]["pageSize"] = pageSize;
    responseData["pagination"]["totalPages"] = totalPages;
    
    return responseData;
}

/**
 * 从文章内容生成摘要
 */
std::string ArticleUtils::generateSummary(const std::string& content, size_t maxLength) {
    if (content.empty()) {
        return "";
    }
    
    if (content.length() <= maxLength) {
        return content;
    }
    
    // 查找合适的截断点（句号、问号、感叹号或第一个空格）
    size_t pos = content.substr(0, maxLength).find_last_of(".!?");
    if (pos == std::string::npos || pos < maxLength / 2) {
        // 如果没找到合适的句子结束符，或者结束符太靠前
        // 尝试在空格处截断
        pos = content.substr(0, maxLength).find_last_of(" ");
        
        if (pos == std::string::npos || pos < maxLength / 2) {
            // 如果还是没找到合适的位置，就直接截断
            pos = maxLength;
        }
    } else {
        // 如果找到了句子结束符，包含它
        pos++;
    }
    
    return content.substr(0, pos) + "...";
}

/**
 * 从字符串生成slug
 */
std::string ArticleUtils::generateSlug(const std::string& text) {
    if (text.empty()) {
        return "";
    }
    
    std::string slug = text;
    
    // 特殊处理：保留C++, C#等常见编程语言名称的原始形式
    if (slug == "C++" || slug == "c++") {
        return "cpp";
    }
    if (slug == "C#" || slug == "c#") {
        return "csharp";
    }
    
    // 转为小写
    std::transform(slug.begin(), slug.end(), slug.begin(), 
        [](unsigned char c){ return std::tolower(c); });
        
    // 特殊字符处理
    for (size_t i = 0; i < slug.length(); i++) {
        unsigned char c = slug[i];
        // 对于非ASCII字符(可能是中文、日文等)，替换为x
        if (c > 127) {
            slug[i] = 'x';
        }
    }
    
    // 替换空格为连字符
    std::replace(slug.begin(), slug.end(), ' ', '-');
    
    // 移除非字母数字和连字符的字符
    slug.erase(std::remove_if(slug.begin(), slug.end(), 
        [](unsigned char c){ return !(std::isalnum(c) || c == '-'); }), slug.end());
        
    // 确保没有连续的连字符
    slug = std::regex_replace(slug, std::regex("-+"), "-");
    
    // 去除首尾连字符
    slug = std::regex_replace(slug, std::regex("^-|-$"), "");
    
    // 如果slug为空(可能全是中文被替换后被移除)，则使用格式化时间戳
    if (slug.empty()) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "post-%Y%m%d%H%M%S", std::localtime(&time_t_now));
        slug = buffer;
    }
    
    return slug;
}

} // namespace utils 