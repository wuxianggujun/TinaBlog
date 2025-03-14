#ifndef TINA_BLOG_BLOG_POST_HPP
#define TINA_BLOG_BLOG_POST_HPP

#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <regex>

namespace fs = std::filesystem;

/**
 * @brief 表示一篇博客文章的类
 */
class BlogPost {
public:
    /**
     * @brief 从文件加载博客文章
     * @param path 文章文件路径
     * @return 是否加载成功
     */
    bool loadFromFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        // 获取文件名作为ID (去掉扩展名)
        fs::path filePath(path);
        id_ = filePath.stem().string();
        
        // 解析文章头部元数据
        std::string line;
        bool inHeader = false;
        std::stringstream contentBuffer;
        
        while (std::getline(file, line)) {
            // 检测元数据区域
            if (line == "---") {
                if (!inHeader) {
                    inHeader = true;
                    continue;
                } else {
                    inHeader = false;
                    continue;
                }
            }
            
            if (inHeader) {
                // 解析元数据 (key: value格式)
                size_t separatorPos = line.find(':');
                if (separatorPos != std::string::npos) {
                    std::string key = line.substr(0, separatorPos);
                    std::string value = line.substr(separatorPos + 1);
                    
                    // 移除前后空格
                    key = trim(key);
                    value = trim(value);
                    
                    metadata_[key] = value;
                    
                    // 设置特定字段
                    if (key == "title") title_ = value;
                    else if (key == "author") author_ = value;
                    else if (key == "date") {
                        date_ = value;
                        // 可以转换为时间戳用于排序
                        parseDate(value);
                    }
                    else if (key == "category") category_ = value;
                    else if (key == "tags") parseTags(value);
                }
            } else {
                // 正文内容
                contentBuffer << line << "\n";
            }
        }
        
        content_ = contentBuffer.str();
        
        // 生成摘要 (取前300个字符)
        summary_ = content_.substr(0, 300);
        if (content_.length() > 300) {
            summary_ += "...";
        }
        
        // 解析发布时间
        lastModified_ = fs::last_write_time(filePath);
        
        return true;
    }
    
    /**
     * @brief 将文章保存到文件
     * @param path 保存路径
     * @return 是否保存成功
     */
    bool saveToFile(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        // 写入元数据头
        file << "---\n";
        file << "title: " << title_ << "\n";
        file << "author: " << author_ << "\n";
        file << "date: " << date_ << "\n";
        file << "category: " << category_ << "\n";
        
        // 写入标签
        file << "tags: ";
        for (size_t i = 0; i < tags_.size(); ++i) {
            file << tags_[i];
            if (i < tags_.size() - 1) {
                file << ", ";
            }
        }
        file << "\n";
        
        // 写入其他元数据
        for (const auto& meta : metadata_) {
            if (meta.first != "title" && meta.first != "author" && 
                meta.first != "date" && meta.first != "category" && 
                meta.first != "tags") {
                file << meta.first << ": " << meta.second << "\n";
            }
        }
        file << "---\n\n";
        
        // 写入正文
        file << content_;
        
        return true;
    }
    
    // Getters
    const std::string& getId() const { return id_; }
    const std::string& getTitle() const { return title_; }
    const std::string& getAuthor() const { return author_; }
    const std::string& getDate() const { return date_; }
    const std::string& getCategory() const { return category_; }
    const std::vector<std::string>& getTags() const { return tags_; }
    const std::string& getContent() const { return content_; }
    const std::string& getSummary() const { return summary_; }
    const fs::file_time_type& getLastModified() const { return lastModified_; }
    
    // 创建新文章
    static BlogPost createNew(const std::string& title, const std::string& author, 
                             const std::string& category, const std::string& content) {
        BlogPost post;
        post.title_ = title;
        post.author_ = author;
        post.category_ = category;
        post.content_ = content;
        
        // 生成当前日期
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
#ifdef _WIN32
        localtime_s(&now_tm, &now_time);
#else
        localtime_r(&now_time, &now_tm);
#endif
        char date_str[11];
        std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", &now_tm);
        post.date_ = date_str;
        
        // 生成ID (基于标题的slug)
        post.id_ = createSlug(title);
        
        // 生成摘要
        post.summary_ = content.substr(0, std::min(size_t(300), content.length()));
        if (content.length() > 300) {
            post.summary_ += "...";
        }
        
        post.lastModified_ = fs::file_time_type::clock::now();
        
        return post;
    }
    
    // 转换为HTML (简单实现，实际可能需要Markdown转HTML)
    std::string toHtml() const {
        // 这里简单处理，实际应用中应该使用Markdown库
        std::string html = content_;
        
        // 替换换行符为<br>
        std::regex newline("\\n");
        html = std::regex_replace(html, newline, "<br>\n");
        
        // 替换标题
        std::regex h1("# (.+)");
        html = std::regex_replace(html, h1, "<h1>$1</h1>");
        
        std::regex h2("## (.+)");
        html = std::regex_replace(html, h2, "<h2>$1</h2>");
        
        // 替换强调
        std::regex bold("\\*\\*(.+?)\\*\\*");
        html = std::regex_replace(html, bold, "<strong>$1</strong>");
        
        std::regex italic("\\*(.+?)\\*");
        html = std::regex_replace(html, italic, "<em>$1</em>");
        
        return html;
    }
    
private:
    std::string id_;
    std::string title_;
    std::string author_;
    std::string date_;
    std::string category_;
    std::vector<std::string> tags_;
    std::string content_;
    std::string summary_;
    std::unordered_map<std::string, std::string> metadata_;
    fs::file_time_type lastModified_;
    
    // 解析标签
    void parseTags(const std::string& tagStr) {
        std::stringstream ss(tagStr);
        std::string tag;
        while (std::getline(ss, tag, ',')) {
            tags_.push_back(trim(tag));
        }
    }
    
    // 解析日期
    void parseDate(const std::string& dateStr) {
        // 可以解析日期并转换为时间戳用于排序
        // 简单实现，实际应用可能需要更复杂的日期处理
    }
    
    // 辅助函数：去除字符串前后空格
    static std::string trim(const std::string& str) {
        const auto begin = str.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) return "";
        
        const auto end = str.find_last_not_of(" \t\r\n");
        return str.substr(begin, end - begin + 1);
    }
    
    // 辅助函数：从标题创建URL友好的slug
    static std::string createSlug(const std::string& title) {
        std::string slug = title;
        
        // 转换为小写
        std::transform(slug.begin(), slug.end(), slug.begin(), 
            [](unsigned char c) { return std::tolower(c); });
        
        // 替换非字母数字字符为连字符
        std::regex nonAlphaNum("[^a-z0-9]+");
        slug = std::regex_replace(slug, nonAlphaNum, "-");
        
        // 移除开头和结尾的连字符
        slug = std::regex_replace(slug, std::regex("^-+|-+$"), "");
        
        return slug;
    }
};

#endif // TINA_BLOG_BLOG_POST_HPP 