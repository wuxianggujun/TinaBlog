-- Tina博客系统数据库架构
-- PostgreSQL 17.x

-- 使用UTF8编码，支持中文字符
-- 注意: PostgreSQL默认使用UTF8编码，无需额外设置

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    uuid VARCHAR(36) UNIQUE NOT NULL,
    username VARCHAR(50) UNIQUE NOT NULL,
    password VARCHAR(100) NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    display_name VARCHAR(100),
    bio TEXT,
    avatar VARCHAR(255),
    is_admin BOOLEAN DEFAULT FALSE,
    is_banned BOOLEAN DEFAULT FALSE,
    ban_reason TEXT,
    banned_at TIMESTAMP,
    banned_by VARCHAR(36) REFERENCES users(uuid),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 文章表
CREATE TABLE IF NOT EXISTS articles (
    id SERIAL PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    slug VARCHAR(200) UNIQUE,
    content TEXT NOT NULL,
    summary TEXT,
    user_uuid VARCHAR(36) NOT NULL REFERENCES users(uuid) ON DELETE CASCADE,
    is_published BOOLEAN DEFAULT TRUE,
    views INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 创建文章浏览量触发器
CREATE OR REPLACE FUNCTION update_article_views()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        NEW.views := 0;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER article_insert_trigger
BEFORE INSERT ON articles
FOR EACH ROW
EXECUTE FUNCTION update_article_views();

-- 分类表
CREATE TABLE IF NOT EXISTS categories (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    slug VARCHAR(50) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 文章分类关联表
CREATE TABLE IF NOT EXISTS article_categories (
    article_id INTEGER REFERENCES articles(id) ON DELETE CASCADE,
    category_id INTEGER REFERENCES categories(id) ON DELETE CASCADE,
    PRIMARY KEY (article_id, category_id)
);

-- 标签表
CREATE TABLE IF NOT EXISTS tags (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    slug VARCHAR(50) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 文章标签关联表
CREATE TABLE IF NOT EXISTS article_tags (
    article_id INTEGER REFERENCES articles(id) ON DELETE CASCADE,
    tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (article_id, tag_id)
);

-- 评论表（预留功能）
CREATE TABLE IF NOT EXISTS comments (
    id SERIAL PRIMARY KEY,
    content TEXT NOT NULL,
    article_id INTEGER NOT NULL REFERENCES articles(id) ON DELETE CASCADE,
    user_uuid VARCHAR(36) REFERENCES users(uuid) ON DELETE SET NULL,
    parent_id INTEGER REFERENCES comments(id) ON DELETE CASCADE,
    author_name VARCHAR(50),
    author_email VARCHAR(100),
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 管理员操作日志表
CREATE TABLE IF NOT EXISTS admin_logs (
    id SERIAL PRIMARY KEY,
    action VARCHAR(50) NOT NULL,
    admin_uuid VARCHAR(36) NOT NULL REFERENCES users(uuid),
    target_uuid VARCHAR(36),
    details TEXT,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- 管理员日志表字段注释
COMMENT ON TABLE admin_logs IS '管理员操作日志表，记录管理员的所有操作';
COMMENT ON COLUMN admin_logs.action IS '操作类型，如: DELETE_USER, BAN_USER, UNBAN_USER等';
COMMENT ON COLUMN admin_logs.admin_uuid IS '执行操作的管理员UUID';
COMMENT ON COLUMN admin_logs.target_uuid IS '操作的目标用户UUID，可能为空';
COMMENT ON COLUMN admin_logs.details IS '操作的详细信息，JSON格式或文本描述';

-- 增加文章浏览量的函数
CREATE OR REPLACE FUNCTION increment_article_views(article_id_param INTEGER)
RETURNS VOID AS $$
BEGIN
    UPDATE articles SET views = views + 1 WHERE id = article_id_param;
END;
$$ LANGUAGE plpgsql;

-- 索引优化
CREATE INDEX IF NOT EXISTS idx_articles_user_uuid ON articles(user_uuid);
CREATE INDEX IF NOT EXISTS idx_articles_is_published ON articles(is_published);
CREATE INDEX IF NOT EXISTS idx_articles_created_at ON articles(created_at);
CREATE INDEX IF NOT EXISTS idx_articles_views ON articles(views);
CREATE INDEX IF NOT EXISTS idx_article_categories_article_id ON article_categories(article_id);
CREATE INDEX IF NOT EXISTS idx_article_categories_category_id ON article_categories(category_id);
CREATE INDEX IF NOT EXISTS idx_article_tags_article_id ON article_tags(article_id);
CREATE INDEX IF NOT EXISTS idx_article_tags_tag_id ON article_tags(tag_id);
CREATE INDEX IF NOT EXISTS idx_comments_article_id ON comments(article_id);
CREATE INDEX IF NOT EXISTS idx_comments_user_uuid ON comments(user_uuid);
CREATE INDEX IF NOT EXISTS idx_admin_logs_admin_uuid ON admin_logs(admin_uuid);
CREATE INDEX IF NOT EXISTS idx_admin_logs_created_at ON admin_logs(created_at);

-- 用户外链表（新增）
CREATE TABLE IF NOT EXISTS user_links (
    id SERIAL PRIMARY KEY,
    user_uuid VARCHAR(36) NOT NULL REFERENCES users(uuid) ON DELETE CASCADE,
    link_type VARCHAR(50) NOT NULL,
    link_url VARCHAR(255) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_uuid, link_type)
);

-- 用户外链表字段注释
COMMENT ON TABLE user_links IS '用户外部链接表，存储用户的社交媒体链接等';
COMMENT ON COLUMN user_links.user_uuid IS '关联的用户UUID';
COMMENT ON COLUMN user_links.link_type IS '链接类型，如: github, website, twitter, weibo等';
COMMENT ON COLUMN user_links.link_url IS '链接URL';

-- 新增用户外链表索引
CREATE INDEX IF NOT EXISTS idx_user_links_user_uuid ON user_links(user_uuid);
CREATE INDEX IF NOT EXISTS idx_user_links_link_type ON user_links(link_type);

-- 为user_links表创建更新时间触发器
CREATE TRIGGER user_links_update_timestamp
BEFORE UPDATE ON user_links
FOR EACH ROW
EXECUTE FUNCTION update_timestamp();

-- 创建更新时间触发器
CREATE OR REPLACE FUNCTION update_timestamp()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER users_update_timestamp
BEFORE UPDATE ON users
FOR EACH ROW
EXECUTE FUNCTION update_timestamp();

CREATE TRIGGER articles_update_timestamp
BEFORE UPDATE ON articles
FOR EACH ROW
EXECUTE FUNCTION update_timestamp();

CREATE TRIGGER categories_update_timestamp
BEFORE UPDATE ON categories
FOR EACH ROW
EXECUTE FUNCTION update_timestamp();

CREATE TRIGGER tags_update_timestamp
BEFORE UPDATE ON tags
FOR EACH ROW
EXECUTE FUNCTION update_timestamp();

CREATE TRIGGER comments_update_timestamp
BEFORE UPDATE ON comments
FOR EACH ROW
EXECUTE FUNCTION update_timestamp();

-- 为表和列添加中文注释
-- users表注释
COMMENT ON TABLE users IS '用户表，存储所有用户信息';
COMMENT ON COLUMN users.id IS '用户ID，自增主键';
COMMENT ON COLUMN users.uuid IS '用户UUID，全局唯一标识符';
COMMENT ON COLUMN users.username IS '用户名，用于登录';
COMMENT ON COLUMN users.password IS '密码哈希值';
COMMENT ON COLUMN users.email IS '电子邮箱';
COMMENT ON COLUMN users.display_name IS '显示名称，用于前端展示';
COMMENT ON COLUMN users.bio IS '个人简介';
COMMENT ON COLUMN users.avatar IS '头像URL';
COMMENT ON COLUMN users.is_admin IS '是否为管理员';
COMMENT ON COLUMN users.is_banned IS '是否被封禁';
COMMENT ON COLUMN users.ban_reason IS '封禁原因';
COMMENT ON COLUMN users.banned_at IS '封禁时间';
COMMENT ON COLUMN users.banned_by IS '执行封禁的管理员UUID';
COMMENT ON COLUMN users.created_at IS '创建时间';
COMMENT ON COLUMN users.updated_at IS '更新时间';

-- user_links表注释
COMMENT ON TABLE user_links IS '用户外部链接表，存储用户的社交媒体等链接';
COMMENT ON COLUMN user_links.id IS '链接ID，自增主键';
COMMENT ON COLUMN user_links.user_uuid IS '用户UUID，关联到users表';
COMMENT ON COLUMN user_links.link_type IS '链接类型，如github、website等';
COMMENT ON COLUMN user_links.link_url IS '链接URL';
COMMENT ON COLUMN user_links.created_at IS '创建时间';
COMMENT ON COLUMN user_links.updated_at IS '更新时间';

-- articles表注释
COMMENT ON TABLE articles IS '文章表，存储博客文章内容';
COMMENT ON COLUMN articles.id IS '文章ID，自增主键';
COMMENT ON COLUMN articles.title IS '文章标题';
COMMENT ON COLUMN articles.slug IS '文章别名，用于URL';
COMMENT ON COLUMN articles.content IS '文章内容';
COMMENT ON COLUMN articles.summary IS '文章摘要';
COMMENT ON COLUMN articles.user_uuid IS '作者UUID，关联到users表';
COMMENT ON COLUMN articles.is_published IS '是否发布';
COMMENT ON COLUMN articles.views IS '浏览量';
COMMENT ON COLUMN articles.created_at IS '创建时间';
COMMENT ON COLUMN articles.updated_at IS '更新时间';

-- categories表注释
COMMENT ON TABLE categories IS '分类表，存储文章分类';
COMMENT ON COLUMN categories.id IS '分类ID，自增主键';
COMMENT ON COLUMN categories.name IS '分类名称';
COMMENT ON COLUMN categories.slug IS '分类别名，用于URL';
COMMENT ON COLUMN categories.created_at IS '创建时间';
COMMENT ON COLUMN categories.updated_at IS '更新时间';

-- article_categories表注释
COMMENT ON TABLE article_categories IS '文章分类关联表，多对多关系';
COMMENT ON COLUMN article_categories.article_id IS '文章ID，关联到articles表';
COMMENT ON COLUMN article_categories.category_id IS '分类ID，关联到categories表';

-- tags表注释
COMMENT ON TABLE tags IS '标签表，存储文章标签';
COMMENT ON COLUMN tags.id IS '标签ID，自增主键';
COMMENT ON COLUMN tags.name IS '标签名称';
COMMENT ON COLUMN tags.slug IS '标签别名，用于URL';
COMMENT ON COLUMN tags.created_at IS '创建时间';
COMMENT ON COLUMN tags.updated_at IS '更新时间';

-- article_tags表注释
COMMENT ON TABLE article_tags IS '文章标签关联表，多对多关系';
COMMENT ON COLUMN article_tags.article_id IS '文章ID，关联到articles表';
COMMENT ON COLUMN article_tags.tag_id IS '标签ID，关联到tags表';

-- comments表注释
COMMENT ON TABLE comments IS '评论表，存储文章评论';
COMMENT ON COLUMN comments.id IS '评论ID，自增主键';
COMMENT ON COLUMN comments.content IS '评论内容';
COMMENT ON COLUMN comments.article_id IS '文章ID，关联到articles表';
COMMENT ON COLUMN comments.user_uuid IS '评论者UUID，关联到users表，可为NULL（游客评论）';
COMMENT ON COLUMN comments.parent_id IS '父评论ID，用于回复功能，关联到comments表';
COMMENT ON COLUMN comments.author_name IS '评论者名称，用于游客评论';
COMMENT ON COLUMN comments.author_email IS '评论者邮箱，用于游客评论';
COMMENT ON COLUMN comments.created_at IS '创建时间';
COMMENT ON COLUMN comments.updated_at IS '更新时间';

-- admin_logs表注释
COMMENT ON TABLE admin_logs IS '管理员操作日志表，记录管理操作';
COMMENT ON COLUMN admin_logs.id IS '日志ID，自增主键';
COMMENT ON COLUMN admin_logs.action IS '操作类型，如DELETE_USER、BAN_USER等';
COMMENT ON COLUMN admin_logs.admin_uuid IS '管理员UUID，关联到users表';
COMMENT ON COLUMN admin_logs.target_uuid IS '操作目标UUID';
COMMENT ON COLUMN admin_logs.details IS '操作详情，JSON格式';
COMMENT ON COLUMN admin_logs.created_at IS '创建时间'; 