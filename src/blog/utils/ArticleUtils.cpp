#include "ArticleUtils.hpp"
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/Field.h>
#include <algorithm>
#include <cctype>
#include <regex>
#include <chrono>
#include <iostream>
#include <unordered_map>

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
    
    // 使用实际的记录数和页数，不再强制设置最小值
    // 如果没有获取到总数，就使用当前结果集的大小
    if (totalCount == 0) {
        totalCount = result.size();
    }
    
    // 计算真实的总页数
    int totalPages = (totalCount + pageSize - 1) / pageSize;
    
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
    
    // 使用简单的中文转拼音映射
    // 常见中文字符的拼音首字母映射表
    static const std::unordered_map<unsigned char, std::string> chineseToPinyin = {
        // 简体中文常用汉字拼音首字母映射
        {0xB0, "a"}, {0xB1, "b"}, {0xB2, "c"}, {0xB3, "d"}, {0xB4, "e"}, 
        {0xB5, "f"}, {0xB6, "g"}, {0xB7, "h"}, {0xB8, "j"}, {0xB9, "k"},
        {0xBA, "l"}, {0xBB, "m"}, {0xBC, "n"}, {0xBD, "o"}, {0xBE, "p"}, 
        {0xBF, "q"}, {0xC0, "r"}, {0xC1, "s"}, {0xC2, "t"}, {0xC3, "w"},
        {0xC4, "x"}, {0xC5, "y"}, {0xC6, "z"}
    };
    
    // 处理中文字符
    std::string processed;
    for (size_t i = 0; i < slug.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(slug[i]);
        // 对于UTF-8中文字符
        if (c > 127) {
            // 尝试使用简单规则将中文转为拼音首字母
            if (i + 1 < slug.length() && (c & 0xE0) == 0xC0) {
                // 双字节UTF-8
                unsigned char next = static_cast<unsigned char>(slug[i + 1]);
                auto it = chineseToPinyin.find(next);
                if (it != chineseToPinyin.end()) {
                    processed += it->second;
                } else {
                    processed += "z"; // 默认值
                }
                i++; // 跳过下一个字节
            } else if (i + 2 < slug.length() && (c & 0xF0) == 0xE0) {
                // 三字节UTF-8，大多数中文
                processed += "z"; // 简单处理
                i += 2; // 跳过接下来的两个字节
            } else {
                // 其他情况，简单处理
                processed += "z";
                // 跳过剩余的UTF-8字节
                while (i + 1 < slug.length() && (static_cast<unsigned char>(slug[i + 1]) & 0xC0) == 0x80) {
                    i++;
                }
            }
        } else if (std::isalnum(c) || c == '-') {
            // 保留字母、数字和连字符
            processed += c;
        } else if (c == ' ') {
            // 空格转换为连字符
            processed += '-';
        }
    }
    
    slug = processed;
    
    // 确保没有连续的连字符
    slug = std::regex_replace(slug, std::regex("-+"), "-");
    
    // 去除首尾连字符
    slug = std::regex_replace(slug, std::regex("^-|-$"), "");
    
    // 如果slug为空，则使用格式化时间戳
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