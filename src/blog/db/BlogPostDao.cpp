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
// 这个方法不再需要，将在每个具体实现中使用ConnectionWrapper
// 删除这个方法，并修改各个使用的地方

// 获取所有文章
std::vector<BlogPostRecord> BlogPostDao::getAllPosts(int limit, int offset) {
    std::vector<BlogPostRecord> result;
    
    try {
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
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
        
        mysqlx::RowResult rows = conn->sql(query).execute();
        
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        std::string query =
            "SELECT id, title, slug, summary, content, "
            "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
            "DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
            "published, view_count "
            "FROM posts "
            "WHERE id = ?";
        
        mysqlx::SqlStatement stmt = conn->sql(query);
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        std::string query =
            "SELECT id, title, slug, summary, content, "
            "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') as created_at, "
            "DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') as updated_at, "
            "published, view_count "
            "FROM posts "
            "WHERE slug = ?";
        
        mysqlx::SqlStatement stmt = conn->sql(query);
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
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
        
        mysqlx::SqlStatement stmt = conn->sql(query);
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
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
        
        mysqlx::SqlStatement stmt = conn->sql(query);
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
    const std::string& author,
    const std::vector<std::string>& categories,
    const std::vector<std::string>& tags,
    bool published
) {
    try {
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        // 生成slug
        std::string slug = generateSlug(title);
        
        // SQL 插入语句添加 slug 字段,插入文章
        std::string query = "INSERT INTO posts (title, slug, content, summary, author, published) "
                         "VALUES (?, ?, ?, ?, ?, ?)";
        
        
        mysqlx::SqlStatement stmt = conn->sql(query);
        stmt.bind(title, slug, content, summary,author, published).execute();
        
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        // 更新文章
        std::string query =
            "UPDATE posts SET "
            "title = ?, "
            "content = ?, "
            "summary = ?, "
            "published = ? "
            "WHERE id = ?";
        
        mysqlx::SqlStatement stmt = conn->sql(query);
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        // 先删除已有的分类关联
        std::string deleteQuery = "DELETE FROM post_categories WHERE post_id = ?";
        conn->sql(deleteQuery).bind(postId).execute();
        
        // 添加新的分类关联
        for (const std::string& category : categories) {
            int categoryId = getOrCreateCategory(category);
            if (categoryId > 0) {
                std::string insertQuery = 
                    "INSERT INTO post_categories (post_id, category_id) VALUES (?, ?)";
                conn->sql(insertQuery).bind(postId, categoryId).execute();
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        // 先删除已有的标签关联
        std::string deleteQuery = "DELETE FROM post_tags WHERE post_id = ?";
        conn->sql(deleteQuery).bind(postId).execute();
        
        // 添加新的标签关联
        for (const std::string& tag : tags) {
            int tagId = getOrCreateTag(tag);
            if (tagId > 0) {
                std::string insertQuery = 
                    "INSERT INTO post_tags (post_id, tag_id) VALUES (?, ?)";
                conn->sql(insertQuery).bind(postId, tagId).execute();
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        std::string query = "UPDATE posts SET view_count = view_count + 1 WHERE id = ?";
        conn->sql(query).bind(id).execute();
        
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        // 删除文章（级联删除会自动删除相关的分类和标签关联）
        std::string query = "DELETE FROM posts WHERE id = ?";
        mysqlx::SqlResult result = conn->sql(query).bind(id).execute();
        
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        std::string query =
            "SELECT c.name FROM categories c "
            "JOIN post_categories pc ON c.id = pc.category_id "
            "WHERE pc.post_id = ?";
        
        mysqlx::SqlResult res = conn->sql(query).bind(post.id).execute();
        
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        std::string query =
            "SELECT t.name FROM tags t "
            "JOIN post_tags pt ON t.id = pt.tag_id "
            "WHERE pt.post_id = ?";
        
        mysqlx::SqlResult res = conn->sql(query).bind(post.id).execute();
        
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        // 先查找是否已存在
        std::string selectQuery = "SELECT id FROM categories WHERE name = ?";
        mysqlx::SqlResult selectResult = conn->sql(selectQuery).bind(name).execute();
        
        if (mysqlx::Row row = selectResult.fetchOne()) {
            return row[0].get<int>();
        }
        
        // 不存在，创建新分类
        std::string slug = generateSlug(name);
        std::string insertQuery = 
            "INSERT INTO categories (name, slug) VALUES (?, ?)";
        mysqlx::SqlStatement stmt = conn->sql(insertQuery);
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
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        // 先查找是否已存在
        std::string selectQuery = "SELECT id FROM tags WHERE name = ?";
        mysqlx::SqlResult selectResult = conn->sql(selectQuery).bind(name).execute();
        
        if (mysqlx::Row row = selectResult.fetchOne()) {
            return row[0].get<int>();
        }
        
        // 不存在，创建新标签
        std::string slug = generateSlug(name);
        std::string insertQuery = 
            "INSERT INTO tags (name, slug) VALUES (?, ?)";
        mysqlx::SqlStatement stmt = conn->sql(insertQuery);
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
    // 生成唯一 slug（从标题转换）
    std::string slug = createSlugFromTitle(name);
    // 检查 slug 是否存在
    if (isSlugExists(slug)) {
        // 添加随机字符确保唯一性
        slug += "-" + generateRandomString(5);
    }
    return slug;
}

// 获取文章总数
int BlogPostDao::getPostCount() {
    try {
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        auto schema = conn->getSchema("blog");
        auto table = schema.getTable("blog_posts");
        
        // 执行COUNT查询
        auto result = table.select("COUNT(*) as count").execute();
        auto row = result.fetchOne();
        
        if (row) {
            return static_cast<int>(row[0]);
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "获取文章总数异常: %s", e.what());
        return 0;
    }
}

// 获取已发布文章数
int BlogPostDao::getPublishedPostCount() {
    try {
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        auto schema = conn->getSchema("blog");
        auto table = schema.getTable("blog_posts");
        
        // 执行COUNT查询，带条件
        auto result = table.select("COUNT(*) as count")
                          .where("published = 1")
                          .execute();
        auto row = result.fetchOne();
        
        if (row) {
            return static_cast<int>(row[0]);
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "获取已发布文章数异常: %s", e.what());
        return 0;
    }
}

// 获取总浏览量
int BlogPostDao::getTotalViewCount() {
    try {
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        auto schema = conn->getSchema("blog");
        auto table = schema.getTable("blog_posts");
        
        // 执行SUM查询
        auto result = table.select("SUM(view_count) as total_views").execute();
        auto row = result.fetchOne();
        
        if (row && !row[0].isNull()) {
            return static_cast<int>(row[0]);
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "获取总浏览量异常: %s", e.what());
        return 0;
    }
}

// 获取分类数量
int BlogPostDao::getCategoryCount() {
    try {
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        auto schema = conn->getSchema("blog");
        auto table = schema.getTable("blog_categories");
        
        // 执行COUNT查询
        auto result = table.select("COUNT(*) as count").execute();
        auto row = result.fetchOne();
        
        if (row) {
            return static_cast<int>(row[0]);
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "获取分类数量异常: %s", e.what());
        return 0;
    }
}

// 获取标签数量
int BlogPostDao::getTagCount() {
    try {
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        auto schema = conn->getSchema("blog");
        auto table = schema.getTable("blog_tags");
        
        // 执行COUNT查询
        auto result = table.select("COUNT(*) as count").execute();
        auto row = result.fetchOne();
        
        if (row) {
            return static_cast<int>(row[0]);
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, 
                     "获取标签数量异常: %s", e.what());
        return 0;
    }
}

// 辅助函数：从标题生成 slug
std::string BlogPostDao::createSlugFromTitle(const std::string& title) {
    std::string slug = title;
    
    // 转为小写
    std::transform(slug.begin(), slug.end(), slug.begin(), 
                   [](const unsigned char c){ return std::tolower(c); });
    
    // 替换空格为连字符
    std::replace(slug.begin(), slug.end(), ' ', '-');
    
    // 移除非字母数字和连字符字符
    slug.erase(std::remove_if(slug.begin(), slug.end(), 
                             [](unsigned char c){ 
                                return !(std::isalnum(c) || c == '-'); 
                             }), 
              slug.end());
    
    return slug;
}

// 检查 slug 是否已存在
bool BlogPostDao::isSlugExists(const std::string& slug) {
    try {
        ConnectionWrapper conn(DbManager::getInstance().getPool());
        
        auto result = conn->sql("SELECT COUNT(*) FROM posts WHERE slug = ?")
                      .bind(slug)
                      .execute();
        
        auto row = result.fetchOne();
        return row[0].get<int>() > 0;
    } catch (const std::exception& e) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "Error checking slug: %s", e.what());
        return false;
    }
}

// 生成随机字符串（用于确保 slug 唯一性）
std::string BlogPostDao::generateRandomString(size_t length) {
    static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string result;
    result.reserve(length);
    
    // 使用当前时间作为随机种子
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[std::rand() % (sizeof(charset) - 1)];
    }
    
    return result;
}