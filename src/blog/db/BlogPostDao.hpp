#pragma once

#include "Nginx.hpp"
#include "DbManager.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <mysqlx/xdevapi.h>

// MySQL类型已经在DbManager.hpp中定义，这里不需要重复定义

/**
 * 博客文章结构
 * 表示数据库中的文章记录
 */
struct BlogPostRecord {
    int id;                    // 文章ID
    std::string title;         // 标题
    std::string slug;          // URL友好的标识符
    std::string content;       // 内容
    std::string summary;       // 摘要
    std::string author;        // 作者
    std::string created_at;    // 创建时间
    std::string updated_at;    // 更新时间
    bool published;            // 是否发布
    int view_count;            // 浏览次数
    std::vector<std::string> categories; // 分类
    std::vector<std::string> tags;       // 标签
};

/**
 * 博客文章数据访问对象
 * 负责文章数据的增删改查操作
 */
class BlogPostDao {
public:
    /**
     * 构造函数
     */
    BlogPostDao();
    
    /**
     * 获取所有文章
     * @param limit 限制条数，默认0表示不限制
     * @param offset 偏移量，默认0表示从头开始
     * @return 文章列表
     */
    std::vector<BlogPostRecord> getAllPosts(int limit = 0, int offset = 0);
    
    /**
     * 按ID获取文章
     * @param id 文章ID
     * @return 文章记录，如果不存在则返回空
     */
    std::optional<BlogPostRecord> getPostById(int id);
    
    /**
     * 按Slug获取文章
     * @param slug 文章slug
     * @return 文章记录，如果不存在则返回空
     */
    std::optional<BlogPostRecord> getPostBySlug(const std::string& slug);
    
    /**
     * 按分类获取文章
     * @param category 分类名
     * @param limit 限制条数
     * @param offset 偏移量
     * @return 文章列表
     */
    std::vector<BlogPostRecord> getPostsByCategory(const std::string& category, int limit = 0, int offset = 0);
    
    /**
     * 按标签获取文章
     * @param tag 标签名
     * @param limit 限制条数
     * @param offset 偏移量
     * @return 文章列表
     */
    std::vector<BlogPostRecord> getPostsByTag(const std::string& tag, int limit = 0, int offset = 0);
    
    /**
     * 创建新文章
     * @param title 标题
     * @param content 内容
     * @param summary 摘要，默认为空
     * @param author 作者
     * @param categories 分类列表
     * @param tags 标签列表
     * @param published 是否发布，默认为true
     * @return 创建的文章ID，失败时返回-1
     */
    int createPost(
        const std::string& title,
        const std::string& content,
        const std::string& summary = "",
        const std::string& author = "null",
        const std::vector<std::string>& categories = {},
        const std::vector<std::string>& tags = {},
        bool published = true
    );
    
    /**
     * 更新文章
     * @param id 文章ID
     * @param title 标题
     * @param content 内容
     * @param summary 摘要
     * @param published 是否发布
     * @return 是否更新成功
     */
    bool updatePost(
        int id,
        const std::string& title,
        const std::string& content,
        const std::string& summary,
        bool published
    );
    
    /**
     * 设置文章分类
     * @param postId 文章ID
     * @param categories 分类列表
     * @return 是否设置成功
     */
    bool setPostCategories(int postId, const std::vector<std::string>& categories);
    
    /**
     * 设置文章标签
     * @param postId 文章ID
     * @param tags 标签列表
     * @return 是否设置成功
     */
    bool setPostTags(int postId, const std::vector<std::string>& tags);
    
    /**
     * 增加文章浏览次数
     * @param id 文章ID
     * @return 是否更新成功
     */
    bool incrementViewCount(int id);
    
    /**
     * 删除文章
     * @param id 文章ID
     * @return 是否删除成功
     */
    bool deletePost(int id);
    
    /**
     * 获取文章总数
     * @return 文章总数
     */
    int getPostCount();
    
    /**
     * 获取已发布文章数
     * @return 已发布文章数
     */
    int getPublishedPostCount();
    
    /**
     * 获取总浏览量
     * @return 总浏览量
     */
    int getTotalViewCount();
    
    /**
     * 获取分类数量
     * @return 分类数量
     */
    int getCategoryCount();
    
    /**
     * 获取标签数量
     * @return 标签数量
     */
    int getTagCount();

private:
    /**
     * 从行结果中解析文章记录
     * @param row MySQL行对象
     * @return 文章记录
     */
    BlogPostRecord parsePostRecord(const mysqlx::Row& row);
    
    /**
     * 加载文章的分类
     * @param post 文章记录
     */
    void loadPostCategories(BlogPostRecord& post);
    
    /**
     * 加载文章的标签
     * @param post 文章记录
     */
    void loadPostTags(BlogPostRecord& post);
    
    /**
     * 获取或创建分类
     * @param name 分类名称
     * @return 分类ID
     */
    int getOrCreateCategory(const std::string& name);
    
    /**
     * 获取或创建标签
     * @param name 标签名称
     * @return 标签ID
     */
    int getOrCreateTag(const std::string& name);
    
    /**
     * 生成URL友好的slug
     * @param name 原始字符串
     * @return slug
     */
    std::string generateSlug(const std::string& name);
    
    /**
     * 获取数据库会话
     * @return 数据库会话
     */
    mysqlx::Session& getSession();

    std::string createSlugFromTitle(const std::string& title);
    bool isSlugExists(const std::string& slug);
    std::string BlogPostDao::generateRandomString(size_t length);
};