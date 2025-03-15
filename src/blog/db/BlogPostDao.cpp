#include "BlogPostDao.hpp"
#include "DbManager.hpp"
#if defined(HAVE_MYSQL) && HAVE_MYSQL
// MySQL头文件已经在DbManager.hpp中包含，这里不需要重复包含
#endif
#include <ngx_core.h>
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

// 工具函数：将字符串转换为小写
static std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

// 工具函数：转义SQL字符串，防止SQL注入
static std::string escapeSqlString(const std::string& str) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    MYSQL* conn = DbManager::getInstance().getConnection();
    if (!conn) return "";
    
    char* escaped = new char[str.length() * 2 + 1];
    mysql_real_escape_string(conn, escaped, str.c_str(), str.length());
    std::string result(escaped);
    delete[] escaped;
    return result;
#else
    // 替代实现：简单替换单引号为两个单引号
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find('\'', pos)) != std::string::npos) {
        result.replace(pos, 1, "''");
        pos += 2;  // 跳过刚插入的两个单引号
    }
    return result;
#endif
}

// 构造函数
BlogPostDao::BlogPostDao() {
    // 没有特殊初始化需求
}

// 执行查询，获取记录集
MYSQL_RES* BlogPostDao::executeQuery(const std::string& sql) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    return DbManager::getInstance().executeQuery(sql);
#else
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "MySQL未启用，无法执行查询: %s", sql.c_str());
    return nullptr;
#endif
}

// 获取所有文章
std::vector<BlogPostRecord> BlogPostDao::getAllPosts(int limit, int offset) {
    std::vector<BlogPostRecord> result;
    
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    std::string query =
        "SELECT id, title, slug, summary, content, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
        "DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
        "published, view_count "
        "FROM posts "
        "WHERE published = TRUE "
        "ORDER BY created_at DESC";
    
    // 添加分页
    if (limit > 0) {
        query += " LIMIT " + std::to_string(limit);
        
        if (offset > 0) {
            query += " OFFSET " + std::to_string(offset);
        }
    }
    
    MYSQL_RES* queryResult = executeQuery(query);
    if (!queryResult) {
        return result;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(queryResult))) {
        BlogPostRecord post;
        unsigned long* lengths = mysql_fetch_lengths(queryResult);
        
        post.id = std::stoi(row[0]);
        post.title = std::string(row[1], lengths[1]);
        post.slug = std::string(row[2], lengths[2]);
        post.summary = row[3] ? std::string(row[3], lengths[3]) : "";
        post.content = std::string(row[4], lengths[4]);
        post.created_at = std::string(row[5], lengths[5]);
        post.updated_at = std::string(row[6], lengths[6]);
        post.published = std::string(row[7], lengths[7]) == "1";
        post.view_count = std::stoi(row[8]);
        
        // 加载分类和标签
        loadPostCategories(post);
        loadPostTags(post);
        
        result.push_back(post);
    }
    
    mysql_free_result(queryResult);
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法获取文章列表");
#endif
    return result;
}

// 按ID获取文章
std::optional<BlogPostRecord> BlogPostDao::getPostById(int id) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    std::string query =
        "SELECT id, title, slug, summary, content, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
        "DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
        "published, view_count "
        "FROM posts "
        "WHERE id = " + std::to_string(id);
    
    MYSQL_RES* queryResult = executeQuery(query);
    if (!queryResult) {
        return std::nullopt;
    }
    
    MYSQL_ROW row = mysql_fetch_row(queryResult);
    if (!row) {
        mysql_free_result(queryResult);
        return std::nullopt;
    }
    
    unsigned long* lengths = mysql_fetch_lengths(queryResult);
    BlogPostRecord post;
    
    post.id = std::stoi(row[0]);
    post.title = std::string(row[1], lengths[1]);
    post.slug = std::string(row[2], lengths[2]);
    post.summary = row[3] ? std::string(row[3], lengths[3]) : "";
    post.content = std::string(row[4], lengths[4]);
    post.created_at = std::string(row[5], lengths[5]);
    post.updated_at = std::string(row[6], lengths[6]);
    post.published = std::string(row[7], lengths[7]) == "1";
    post.view_count = std::stoi(row[8]);
    
    // 加载分类和标签
    loadPostCategories(post);
    loadPostTags(post);
    
    mysql_free_result(queryResult);
    return post;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法按ID获取文章: %d", id);
    return std::nullopt;
#endif
}

// 按Slug获取文章
std::optional<BlogPostRecord> BlogPostDao::getPostBySlug(const std::string& slug) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    std::string escapedSlug = escapeSqlString(slug);
    std::string query =
        "SELECT id, title, slug, summary, content, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
        "DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
        "published, view_count "
        "FROM posts "
        "WHERE slug = '" + escapedSlug + "'";
    
    MYSQL_RES* queryResult = executeQuery(query);
    if (!queryResult) {
        return std::nullopt;
    }
    
    MYSQL_ROW row = mysql_fetch_row(queryResult);
    if (!row) {
        mysql_free_result(queryResult);
        return std::nullopt;
    }
    
    unsigned long* lengths = mysql_fetch_lengths(queryResult);
    BlogPostRecord post;
    
    post.id = std::stoi(row[0]);
    post.title = std::string(row[1], lengths[1]);
    post.slug = std::string(row[2], lengths[2]);
    post.summary = row[3] ? std::string(row[3], lengths[3]) : "";
    post.content = std::string(row[4], lengths[4]);
    post.created_at = std::string(row[5], lengths[5]);
    post.updated_at = std::string(row[6], lengths[6]);
    post.published = std::string(row[7], lengths[7]) == "1";
    post.view_count = std::stoi(row[8]);
    
    // 加载分类和标签
    loadPostCategories(post);
    loadPostTags(post);
    
    mysql_free_result(queryResult);
    return post;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法按Slug获取文章: %s", slug.c_str());
    return std::nullopt;
#endif
}

// 按分类获取文章
std::vector<BlogPostRecord> BlogPostDao::getPostsByCategory(const std::string& category, int limit, int offset) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    std::vector<BlogPostRecord> result;
    std::string escapedCategory = escapeSqlString(category);
    
    std::string query =
        "SELECT p.id, p.title, p.slug, p.summary, p.content, "
        "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
        "DATE_FORMAT(p.updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
        "p.published, p.view_count "
        "FROM posts p "
        "JOIN post_categories pc ON p.id = pc.post_id "
        "JOIN categories c ON pc.category_id = c.id "
        "WHERE p.published = TRUE AND c.name = '" + escapedCategory + "' "
        "ORDER BY p.created_at DESC";
    
    // 添加分页
    if (limit > 0) {
        query += " LIMIT " + std::to_string(limit);
        
        if (offset > 0) {
            query += " OFFSET " + std::to_string(offset);
        }
    }
    
    MYSQL_RES* queryResult = executeQuery(query);
    if (!queryResult) {
        return result;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(queryResult))) {
        BlogPostRecord post;
        unsigned long* lengths = mysql_fetch_lengths(queryResult);
        
        post.id = std::stoi(row[0]);
        post.title = std::string(row[1], lengths[1]);
        post.slug = std::string(row[2], lengths[2]);
        post.summary = row[3] ? std::string(row[3], lengths[3]) : "";
        post.content = std::string(row[4], lengths[4]);
        post.created_at = std::string(row[5], lengths[5]);
        post.updated_at = std::string(row[6], lengths[6]);
        post.published = std::string(row[7], lengths[7]) == "1";
        post.view_count = std::stoi(row[8]);
        
        // 加载分类和标签
        loadPostCategories(post);
        loadPostTags(post);
        
        result.push_back(post);
    }
    
    mysql_free_result(queryResult);
    return result;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法按分类获取文章: %s", category.c_str());
    return std::vector<BlogPostRecord>();
#endif
}

// 按标签获取文章
std::vector<BlogPostRecord> BlogPostDao::getPostsByTag(const std::string& tag, int limit, int offset) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    std::vector<BlogPostRecord> result;
    std::string escapedTag = escapeSqlString(tag);
    
    std::string query =
        "SELECT p.id, p.title, p.slug, p.summary, p.content, "
        "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
        "DATE_FORMAT(p.updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
        "p.published, p.view_count "
        "FROM posts p "
        "JOIN post_tags pt ON p.id = pt.post_id "
        "JOIN tags t ON pt.tag_id = t.id "
        "WHERE p.published = TRUE AND t.name = '" + escapedTag + "' "
        "ORDER BY p.created_at DESC";
    
    // 添加分页
    if (limit > 0) {
        query += " LIMIT " + std::to_string(limit);
        
        if (offset > 0) {
            query += " OFFSET " + std::to_string(offset);
        }
    }
    
    MYSQL_RES* queryResult = executeQuery(query);
    if (!queryResult) {
        return result;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(queryResult))) {
        BlogPostRecord post;
        unsigned long* lengths = mysql_fetch_lengths(queryResult);
        
        post.id = std::stoi(row[0]);
        post.title = std::string(row[1], lengths[1]);
        post.slug = std::string(row[2], lengths[2]);
        post.summary = row[3] ? std::string(row[3], lengths[3]) : "";
        post.content = std::string(row[4], lengths[4]);
        post.created_at = std::string(row[5], lengths[5]);
        post.updated_at = std::string(row[6], lengths[6]);
        post.published = std::string(row[7], lengths[7]) == "1";
        post.view_count = std::stoi(row[8]);
        
        // 加载分类和标签
        loadPostCategories(post);
        loadPostTags(post);
        
        result.push_back(post);
    }
    
    mysql_free_result(queryResult);
    return result;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法按标签获取文章: %s", tag.c_str());
    return std::vector<BlogPostRecord>();
#endif
}

// 创建新文章
int BlogPostDao::createPost(
    const std::string& title,
    const std::string& content,
    const std::string& summary,
    const std::vector<std::string>& categories,
    const std::vector<std::string>& tags,
    bool published
) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    // 生成slug
    std::string slug = generateSlug(title);
    
    // 转义字符串
    std::string escapedTitle = escapeSqlString(title);
    std::string escapedSlug = escapeSqlString(slug);
    std::string escapedContent = escapeSqlString(content);
    std::string escapedSummary = escapeSqlString(summary);
    
    // 插入文章
    std::string query = 
        "INSERT INTO posts(title, slug, content, summary, published) "
        "VALUES('" + escapedTitle + "', '" + escapedSlug + "', '" 
                  + escapedContent + "', '" + escapedSummary + "', " 
                  + (published ? "TRUE" : "FALSE") + ")";
    
    int result = DbManager::getInstance().executeUpdate(query);
    if (result == -1) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "创建文章失败");
        return -1;
    }
    
    // 获取最后插入的ID
    int postId = static_cast<int>(DbManager::getInstance().getLastInsertId());
    
    // 设置分类
    if (!categories.empty() && !setPostCategories(postId, categories)) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "设置文章分类失败, id=%d", postId);
    }
    
    // 设置标签
    if (!tags.empty() && !setPostTags(postId, tags)) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "设置文章标签失败, id=%d", postId);
    }
    
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                 "创建文章成功: id=%d, title=%s", 
                 postId, title.c_str());
    
    return postId;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法创建文章: %s", title.c_str());
    return -1;
#endif
}

// 更新文章
bool BlogPostDao::updatePost(
    int id,
    const std::string& title,
    const std::string& content,
    const std::string& summary,
    bool published
) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    // 转义字符串
    std::string escapedTitle = escapeSqlString(title);
    std::string escapedContent = escapeSqlString(content);
    std::string escapedSummary = escapeSqlString(summary);
    
    // 更新文章
    std::string query = 
        "UPDATE posts SET "
        "title = '" + escapedTitle + "', "
        "content = '" + escapedContent + "', "
        "summary = '" + escapedSummary + "', "
        "published = " + (published ? "TRUE" : "FALSE") + " "
        "WHERE id = " + std::to_string(id);
    
    int result = DbManager::getInstance().executeUpdate(query);
    if (result == -1) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "更新文章失败, id=%d", id);
        return false;
    }
    
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                 "更新文章成功: id=%d", id);
    
    return true;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法更新文章: %d", id);
    return false;
#endif
}

// 设置文章分类
bool BlogPostDao::setPostCategories(int postId, const std::vector<std::string>& categories) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    // 先删除现有分类关联
    std::string deleteQuery = 
        "DELETE FROM post_categories WHERE post_id = " + std::to_string(postId);
    
    if (DbManager::getInstance().executeUpdate(deleteQuery) == -1) {
        return false;
    }
    
    // 添加新分类关联
    for (const auto& category : categories) {
        int categoryId = getOrCreateCategory(category);
        if (categoryId > 0) {
            std::string insertQuery = 
                "INSERT INTO post_categories(post_id, category_id) VALUES(" 
                + std::to_string(postId) + ", " + std::to_string(categoryId) + ")";
            
            if (DbManager::getInstance().executeUpdate(insertQuery) == -1) {
                ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                             "添加文章分类关联失败, post_id=%d, category=%s", 
                             postId, category.c_str());
            }
        }
    }
    
    return true;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法设置文章分类: %d", postId);
    return false;
#endif
}

// 设置文章标签
bool BlogPostDao::setPostTags(int postId, const std::vector<std::string>& tags) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    // 先删除现有标签关联
    std::string deleteQuery = 
        "DELETE FROM post_tags WHERE post_id = " + std::to_string(postId);
    
    if (DbManager::getInstance().executeUpdate(deleteQuery) == -1) {
        return false;
    }
    
    // 添加新标签关联
    for (const auto& tag : tags) {
        int tagId = getOrCreateTag(tag);
        if (tagId > 0) {
            std::string insertQuery = 
                "INSERT INTO post_tags(post_id, tag_id) VALUES(" 
                + std::to_string(postId) + ", " + std::to_string(tagId) + ")";
            
            if (DbManager::getInstance().executeUpdate(insertQuery) == -1) {
                ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                             "添加文章标签关联失败, post_id=%d, tag=%s", 
                             postId, tag.c_str());
            }
        }
    }
    
    return true;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法设置文章标签: %d", postId);
    return false;
#endif
}

// 增加文章浏览次数
bool BlogPostDao::incrementViewCount(int id) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    std::string query = 
        "UPDATE posts SET view_count = view_count + 1 WHERE id = " + std::to_string(id);
    
    return DbManager::getInstance().executeUpdate(query) != -1;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法增加文章浏览次数: %d", id);
    return false;
#endif
}

// 删除文章
bool BlogPostDao::deletePost(int id) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    std::string query = "DELETE FROM posts WHERE id = " + std::to_string(id);
    
    int result = DbManager::getInstance().executeUpdate(query);
    if (result == -1) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "删除文章失败, id=%d", id);
        return false;
    }
    
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, 
                 "删除文章成功: id=%d", id);
    
    return true;
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法删除文章: %d", id);
    return false;
#endif
}

// 加载文章的分类
void BlogPostDao::loadPostCategories(BlogPostRecord& post) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    std::string query = 
        "SELECT c.name FROM categories c "
        "JOIN post_categories pc ON c.id = pc.category_id "
        "WHERE pc.post_id = " + std::to_string(post.id);
    
    MYSQL_RES* queryResult = executeQuery(query);
    if (!queryResult) {
        return;
    }
    
    post.categories.clear();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(queryResult))) {
        unsigned long* lengths = mysql_fetch_lengths(queryResult);
        post.categories.push_back(std::string(row[0], lengths[0]));
    }
    
    mysql_free_result(queryResult);
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法加载文章分类: %d", post.id);
#endif
}

// 加载文章的标签
void BlogPostDao::loadPostTags(BlogPostRecord& post) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    std::string query = 
        "SELECT t.name FROM tags t "
        "JOIN post_tags pt ON t.id = pt.tag_id "
        "WHERE pt.post_id = " + std::to_string(post.id);
    
    MYSQL_RES* queryResult = executeQuery(query);
    if (!queryResult) {
        return;
    }
    
    post.tags.clear();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(queryResult))) {
        unsigned long* lengths = mysql_fetch_lengths(queryResult);
        post.tags.push_back(std::string(row[0], lengths[0]));
    }
    
    mysql_free_result(queryResult);
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法加载文章标签: %d", post.id);
#endif
}

// 获取或创建分类
int BlogPostDao::getOrCreateCategory(const std::string& name) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    if (name.empty()) {
        return -1;
    }
    
    std::string escapedName = escapeSqlString(name);
    std::string slug = generateSlug(name);
    std::string escapedSlug = escapeSqlString(slug);
    
    // 先查询是否存在
    std::string query = 
        "SELECT id FROM categories WHERE name = '" + escapedName + "'";
    
    MYSQL_RES* queryResult = executeQuery(query);
    if (!queryResult) {
        return -1;
    }
    
    MYSQL_ROW row = mysql_fetch_row(queryResult);
    if (row) {
        int id = std::stoi(row[0]);
        mysql_free_result(queryResult);
        return id;
    }
    
    mysql_free_result(queryResult);
    
    // 不存在，则创建
    std::string insertQuery = 
        "INSERT INTO categories(name, slug) VALUES('" 
        + escapedName + "', '" + escapedSlug + "')";
    
    int result = DbManager::getInstance().executeUpdate(insertQuery);
    if (result == -1) {
        return -1;
    }
    
    return static_cast<int>(DbManager::getInstance().getLastInsertId());
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法获取或创建分类: %s", name.c_str());
    return -1;
#endif
}

// 获取或创建标签
int BlogPostDao::getOrCreateTag(const std::string& name) {
#if defined(HAVE_MYSQL) && HAVE_MYSQL
    if (name.empty()) {
        return -1;
    }
    
    std::string escapedName = escapeSqlString(name);
    std::string slug = generateSlug(name);
    std::string escapedSlug = escapeSqlString(slug);
    
    // 先查询是否存在
    std::string query = 
        "SELECT id FROM tags WHERE name = '" + escapedName + "'";
    
    MYSQL_RES* queryResult = executeQuery(query);
    if (!queryResult) {
        return -1;
    }
    
    MYSQL_ROW row = mysql_fetch_row(queryResult);
    if (row) {
        int id = std::stoi(row[0]);
        mysql_free_result(queryResult);
        return id;
    }
    
    mysql_free_result(queryResult);
    
    // 不存在，则创建
    std::string insertQuery = 
        "INSERT INTO tags(name, slug) VALUES('" 
        + escapedName + "', '" + escapedSlug + "')";
    
    int result = DbManager::getInstance().executeUpdate(insertQuery);
    if (result == -1) {
        return -1;
    }
    
    return static_cast<int>(DbManager::getInstance().getLastInsertId());
#else
    ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "MySQL未启用，无法获取或创建标签: %s", name.c_str());
    return -1;
#endif
}

// 生成URL友好的slug
std::string BlogPostDao::generateSlug(const std::string& name) {
    std::string result;
    
    // 转换为小写
    std::string lower = toLower(name);
    
    // 替换空格为连字符
    std::regex nonAlphaNum("[^a-z0-9\\s]");
    std::string alphaNum = std::regex_replace(lower, nonAlphaNum, "");
    
    // 替换空白为连字符
    std::regex spaces("\\s+");
    result = std::regex_replace(alphaNum, spaces, "-");
    
    // 移除首尾连字符
    if (!result.empty() && result[0] == '-') {
        result = result.substr(1);
    }
    
    if (!result.empty() && result[result.length() - 1] == '-') {
        result = result.substr(0, result.length() - 1);
    }
    
    return result;
}