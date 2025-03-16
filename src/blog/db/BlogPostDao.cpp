#include "BlogPostDao.hpp"
#include "DbManager.hpp"
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
    // 使用参数化查询代替手动转义
    // 此函数保留以防有些地方需要直接构建SQL
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find('\'', pos)) != std::string::npos) {
        result.replace(pos, 1, "''");
        pos += 2;  // 跳过刚插入的两个单引号
    }
    return result;
}

// 构造函数
BlogPostDao::BlogPostDao() {
    // 没有特殊初始化需求
}

// 获取数据库会话
mysqlx::Session& BlogPostDao::getSession() {
    return DbManager::getInstance().getSession();
}

// 获取所有文章
std::vector<BlogPostRecord> BlogPostDao::getAllPosts(int limit, int offset) {
    std::vector<BlogPostRecord> result;
    
    try {
        mysqlx::Session& session = getSession();
        
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
        
        mysqlx::RowResult rows = session.sql(query).execute();
        
        for (const mysqlx::Row& row : rows.fetchAll()) {
            BlogPostRecord post = parsePostRecord(row);
            
            // 加载文章的分类和标签
            loadPostCategories(post);
            loadPostTags(post);
            
            result.push_back(post);
        }
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error fetching posts: %s", ex.what());
    }
    
    return result;
}

// 按ID获取文章
std::optional<BlogPostRecord> BlogPostDao::getPostById(int id) {
    try {
        mysqlx::Session& session = getSession();
        
        std::string query =
            "SELECT id, title, slug, summary, content, "
            "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
            "DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
            "published, view_count "
            "FROM posts "
            "WHERE id = ?";
        
        mysqlx::SqlStatement stmt = session.sql(query);
        mysqlx::SqlResult res = stmt.bind(id).execute();
        
        // 获取结果
        if (mysqlx::Row row = res.fetchOne()) {
            BlogPostRecord post = parsePostRecord(row);
            
            // 加载文章的分类和标签
            loadPostCategories(post);
            loadPostTags(post);
            
            return post;
        }
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error fetching post by ID: %s", ex.what());
    }
    
    return std::nullopt;
}

// 按Slug获取文章
std::optional<BlogPostRecord> BlogPostDao::getPostBySlug(const std::string& slug) {
    try {
        mysqlx::Session& session = getSession();
        
        std::string query =
            "SELECT id, title, slug, summary, content, "
            "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
            "DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
            "published, view_count "
            "FROM posts "
            "WHERE slug = ?";
        
        mysqlx::SqlStatement stmt = session.sql(query);
        mysqlx::SqlResult res = stmt.bind(slug).execute();
        
        // 获取结果
        if (mysqlx::Row row = res.fetchOne()) {
            BlogPostRecord post = parsePostRecord(row);
            
            // 加载文章的分类和标签
            loadPostCategories(post);
            loadPostTags(post);
            
            return post;
        }
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error fetching post by slug: %s", ex.what());
    }
    
    return std::nullopt;
}

// 按分类获取文章
std::vector<BlogPostRecord> BlogPostDao::getPostsByCategory(const std::string& category, int limit, int offset) {
    std::vector<BlogPostRecord> result;
    
    try {
        mysqlx::Session& session = getSession();
        
        std::string query =
            "SELECT p.id, p.title, p.slug, p.summary, p.content, "
            "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
            "DATE_FORMAT(p.updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
            "p.published, p.view_count "
            "FROM posts p "
            "JOIN post_categories pc ON p.id = pc.post_id "
            "JOIN categories c ON pc.category_id = c.id "
            "WHERE c.name = ? AND p.published = TRUE "
            "ORDER BY p.created_at DESC";
        
        // 添加分页
        if (limit > 0) {
            query += " LIMIT " + std::to_string(limit);
            
            if (offset > 0) {
                query += " OFFSET " + std::to_string(offset);
            }
        }
        
        mysqlx::SqlStatement stmt = session.sql(query);
        mysqlx::SqlResult res = stmt.bind(category).execute();
        
        for (const mysqlx::Row& row : res.fetchAll()) {
            BlogPostRecord post = parsePostRecord(row);
            
            // 加载文章的分类和标签
            loadPostCategories(post);
            loadPostTags(post);
            
            result.push_back(post);
        }
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error fetching posts by category: %s", ex.what());
    }
    
    return result;
}

// 按标签获取文章
std::vector<BlogPostRecord> BlogPostDao::getPostsByTag(const std::string& tag, int limit, int offset) {
    std::vector<BlogPostRecord> result;
    
    try {
        mysqlx::Session& session = getSession();
        
        std::string query =
            "SELECT p.id, p.title, p.slug, p.summary, p.content, "
            "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
            "DATE_FORMAT(p.updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
            "p.published, p.view_count "
            "FROM posts p "
            "JOIN post_tags pt ON p.id = pt.post_id "
            "JOIN tags t ON pt.tag_id = t.id "
            "WHERE t.name = ? AND p.published = TRUE "
            "ORDER BY p.created_at DESC";
        
        // 添加分页
        if (limit > 0) {
            query += " LIMIT " + std::to_string(limit);
            
            if (offset > 0) {
                query += " OFFSET " + std::to_string(offset);
            }
        }
        
        mysqlx::SqlStatement stmt = session.sql(query);
        mysqlx::SqlResult res = stmt.bind(tag).execute();
        
        for (const mysqlx::Row& row : res.fetchAll()) {
            BlogPostRecord post = parsePostRecord(row);
            
            // 加载文章的分类和标签
            loadPostCategories(post);
            loadPostTags(post);
            
            result.push_back(post);
        }
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error fetching posts by tag: %s", ex.what());
    }
    
    return result;
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
    try {
        mysqlx::Session& session = getSession();
        
        // 生成slug
        std::string slug = generateSlug(title);
        
        // 插入文章
        std::string query =
            "INSERT INTO posts (title, slug, content, summary, published) "
            "VALUES (?, ?, ?, ?, ?)";
        
        mysqlx::SqlStatement stmt = session.sql(query);
        stmt.bind(title, slug, content, summary, published).execute();
        
        // 获取插入的ID
        int postId = static_cast<int>(DbManager::getInstance().getLastInsertId());
        
        if (postId > 0) {
            // 设置分类
            if (!categories.empty()) {
                setPostCategories(postId, categories);
            }
            
            // 设置标签
            if (!tags.empty()) {
                setPostTags(postId, tags);
            }
        }
        
        return postId;
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error creating post: %s", ex.what());
        return -1;
    }
}

// 更新文章
bool BlogPostDao::updatePost(
    int id,
    const std::string& title,
    const std::string& content,
    const std::string& summary,
    bool published
) {
    try {
        mysqlx::Session& session = getSession();
        
        // 更新文章
        std::string query =
            "UPDATE posts SET "
            "title = ?, "
            "content = ?, "
            "summary = ?, "
            "published = ? "
            "WHERE id = ?";
        
        mysqlx::SqlStatement stmt = session.sql(query);
        stmt.bind(title, content, summary, published, id).execute();
        
        return true;
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error updating post: %s", ex.what());
        return false;
    }
}

// 设置文章分类
bool BlogPostDao::setPostCategories(int postId, const std::vector<std::string>& categories) {
    try {
        mysqlx::Session& session = getSession();
        
        // 先删除已有的分类关联
        std::string deleteQuery = "DELETE FROM post_categories WHERE post_id = ?";
        session.sql(deleteQuery).bind(postId).execute();
        
        // 添加新的分类关联
        for (const std::string& category : categories) {
            int categoryId = getOrCreateCategory(category);
            if (categoryId > 0) {
                std::string insertQuery = 
                    "INSERT INTO post_categories (post_id, category_id) VALUES (?, ?)";
                session.sql(insertQuery).bind(postId, categoryId).execute();
            }
        }
        
        return true;
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error setting post categories: %s", ex.what());
        return false;
    }
}

// 设置文章标签
bool BlogPostDao::setPostTags(int postId, const std::vector<std::string>& tags) {
    try {
        mysqlx::Session& session = getSession();
        
        // 先删除已有的标签关联
        std::string deleteQuery = "DELETE FROM post_tags WHERE post_id = ?";
        session.sql(deleteQuery).bind(postId).execute();
        
        // 添加新的标签关联
        for (const std::string& tag : tags) {
            int tagId = getOrCreateTag(tag);
            if (tagId > 0) {
                std::string insertQuery = 
                    "INSERT INTO post_tags (post_id, tag_id) VALUES (?, ?)";
                session.sql(insertQuery).bind(postId, tagId).execute();
            }
        }
        
        return true;
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error setting post tags: %s", ex.what());
        return false;
    }
}

// 增加文章浏览次数
bool BlogPostDao::incrementViewCount(int id) {
    try {
        mysqlx::Session& session = getSession();
        
        std::string query = "UPDATE posts SET view_count = view_count + 1 WHERE id = ?";
        session.sql(query).bind(id).execute();
        
        return true;
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error incrementing view count: %s", ex.what());
        return false;
    }
}

// 删除文章
bool BlogPostDao::deletePost(int id) {
    try {
        mysqlx::Session& session = getSession();
        
        // 删除文章（级联删除会自动删除相关的分类和标签关联）
        std::string query = "DELETE FROM posts WHERE id = ?";
        mysqlx::SqlResult result = session.sql(query).bind(id).execute();
        
        return result.getAffectedItemsCount() > 0;
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error deleting post: %s", ex.what());
        return false;
    }
}

// 从行结果中解析文章记录
BlogPostRecord BlogPostDao::parsePostRecord(const mysqlx::Row& row) {
    BlogPostRecord post;
    
    post.id = row[0].get<int>();
    post.title = row[1].get<std::string>();
    post.slug = row[2].get<std::string>();
    post.summary = row[3].isNull() ? "" : row[3].get<std::string>();
    post.content = row[4].get<std::string>();
    post.created_at = row[5].get<std::string>();
    post.updated_at = row[6].get<std::string>();
    post.published = row[7].get<bool>();
    post.view_count = row[8].get<int>();
    
    return post;
}

// 加载文章的分类
void BlogPostDao::loadPostCategories(BlogPostRecord& post) {
    try {
        mysqlx::Session& session = getSession();
        
        std::string query =
            "SELECT c.name FROM categories c "
            "JOIN post_categories pc ON c.id = pc.category_id "
            "WHERE pc.post_id = ?";
        
        mysqlx::SqlResult res = session.sql(query).bind(post.id).execute();
        
        post.categories.clear();
        for (const mysqlx::Row& row : res.fetchAll()) {
            post.categories.push_back(row[0].get<std::string>());
        }
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error loading post categories: %s", ex.what());
    }
}

// 加载文章的标签
void BlogPostDao::loadPostTags(BlogPostRecord& post) {
    try {
        mysqlx::Session& session = getSession();
        
        std::string query =
            "SELECT t.name FROM tags t "
            "JOIN post_tags pt ON t.id = pt.tag_id "
            "WHERE pt.post_id = ?";
        
        mysqlx::SqlResult res = session.sql(query).bind(post.id).execute();
        
        post.tags.clear();
        for (const mysqlx::Row& row : res.fetchAll()) {
            post.tags.push_back(row[0].get<std::string>());
        }
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error loading post tags: %s", ex.what());
    }
}

// 获取或创建分类
int BlogPostDao::getOrCreateCategory(const std::string& name) {
    try {
        mysqlx::Session& session = getSession();
        
        // 先查找是否已存在
        std::string selectQuery = "SELECT id FROM categories WHERE name = ?";
        mysqlx::SqlResult selectResult = session.sql(selectQuery).bind(name).execute();
        
        if (mysqlx::Row row = selectResult.fetchOne()) {
            return row[0].get<int>();
        }
        
        // 不存在，创建新分类
        std::string slug = generateSlug(name);
        std::string insertQuery = 
            "INSERT INTO categories (name, slug) VALUES (?, ?)";
        mysqlx::SqlStatement stmt = session.sql(insertQuery);
        stmt.bind(name, slug).execute();
        
        // 返回新创建的ID
        return static_cast<int>(DbManager::getInstance().getLastInsertId());
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error getting or creating category: %s", ex.what());
        return -1;
    }
}

// 获取或创建标签
int BlogPostDao::getOrCreateTag(const std::string& name) {
    try {
        mysqlx::Session& session = getSession();
        
        // 先查找是否已存在
        std::string selectQuery = "SELECT id FROM tags WHERE name = ?";
        mysqlx::SqlResult selectResult = session.sql(selectQuery).bind(name).execute();
        
        if (mysqlx::Row row = selectResult.fetchOne()) {
            return row[0].get<int>();
        }
        
        // 不存在，创建新标签
        std::string slug = generateSlug(name);
        std::string insertQuery = 
            "INSERT INTO tags (name, slug) VALUES (?, ?)";
        mysqlx::SqlStatement stmt = session.sql(insertQuery);
        stmt.bind(name, slug).execute();
        
        // 返回新创建的ID
        return static_cast<int>(DbManager::getInstance().getLastInsertId());
    }
    catch (const std::exception& ex) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "Error getting or creating tag: %s", ex.what());
        return -1;
    }
}

// 生成URL友好的slug
std::string BlogPostDao::generateSlug(const std::string& name) {
    // 转为小写
    std::string slug = toLower(name);
    
    // 替换非字母数字字符为连字符
    std::regex nonAlphaNum("[^a-z0-9]+");
    slug = std::regex_replace(slug, nonAlphaNum, "-");
    
    // 去除头尾的连字符
    slug = std::regex_replace(slug, std::regex("^-+|-+$"), "");
    
    return slug;
}