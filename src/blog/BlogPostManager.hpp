#ifndef TINA_BLOG_BLOG_POST_MANAGER_HPP
#define TINA_BLOG_BLOG_POST_MANAGER_HPP

#include "BlogPost.hpp"
#include "BlogConfig.hpp"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <thread>

/**
 * @brief 管理博客文章的类，实现单例模式
 */
class BlogPostManager {
public:
    /**
     * @brief 获取单例实例
     * @return 博客文章管理器实例
     */
    static BlogPostManager& getInstance() {
        static BlogPostManager instance;
        return instance;
    }
    
    /**
     * @brief 初始化文章管理器，加载所有文章
     * @param postsDirectory 文章目录路径
     * @return 是否初始化成功
     */
    bool initialize(const std::string& postsDirectory) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        postsDirectory_ = postsDirectory;
        loaded_ = false;
        
        return reloadPosts();
    }
    
    /**
     * @brief 重新加载所有文章
     * @return 是否加载成功
     */
    bool reloadPosts() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        posts_.clear();
        postsById_.clear();
        postsByCategory_.clear();
        
        if (!fs::exists(postsDirectory_) || !fs::is_directory(postsDirectory_)) {
            return false;
        }
        
        try {
            for (const auto& entry : fs::directory_iterator(postsDirectory_)) {
                if (entry.is_regular_file() && 
                    (entry.path().extension() == ".md" || entry.path().extension() == ".txt")) {
                    BlogPost post;
                    if (post.loadFromFile(entry.path().string())) {
                        posts_.push_back(post);
                        postsById_[post.getId()] = posts_.size() - 1;
                        postsByCategory_[post.getCategory()].push_back(posts_.size() - 1);
                    }
                }
            }
            
            // 按发布日期排序（最新的在前）
            std::sort(posts_.begin(), posts_.end(), [](const BlogPost& a, const BlogPost& b) {
                return a.getLastModified() > b.getLastModified();
            });
            
            // 重建索引
            rebuildIndices();
            
            loaded_ = true;
            return true;
        } catch (const std::exception& /* e */) {
            // 出错处理
            return false;
        }
    }
    
    /**
     * @brief 根据ID获取博客文章
     * @param id 文章ID
     * @return 文章对象指针，如果不存在返回nullptr
     */
    const BlogPost* getPostById(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = postsById_.find(id);
        if (it != postsById_.end() && it->second < posts_.size()) {
            return &posts_[it->second];
        }
        return nullptr;
    }
    
    /**
     * @brief 获取所有文章
     * @param limit 限制返回数量，0表示不限制
     * @param offset 起始位置偏移
     * @return 文章列表
     */
    std::vector<const BlogPost*> getAllPosts(size_t limit = 0, size_t offset = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<const BlogPost*> result;
        size_t count = 0;
        size_t end = limit > 0 ? std::min(offset + limit, posts_.size()) : posts_.size();
        
        for (size_t i = offset; i < end; ++i) {
            result.push_back(&posts_[i]);
        }
        
        return result;
    }
    
    /**
     * @brief 按分类获取文章
     * @param category 分类名称
     * @param limit 限制返回数量，0表示不限制
     * @param offset 起始位置偏移
     * @return 文章列表
     */
    std::vector<const BlogPost*> getPostsByCategory(const std::string& category, 
                                                  size_t limit = 0, 
                                                  size_t offset = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<const BlogPost*> result;
        auto it = postsByCategory_.find(category);
        if (it == postsByCategory_.end()) {
            return result;
        }
        
        const auto& indices = it->second;
        size_t end = limit > 0 ? std::min(offset + limit, indices.size()) : indices.size();
        
        for (size_t i = offset; i < end; ++i) {
            if (indices[i] < posts_.size()) {
                result.push_back(&posts_[indices[i]]);
            }
        }
        
        return result;
    }
    
    /**
     * @brief 获取所有分类
     * @return 分类列表
     */
    std::vector<std::string> getAllCategories() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> categories;
        categories.reserve(postsByCategory_.size());
        
        for (const auto& category : postsByCategory_) {
            categories.push_back(category.first);
        }
        
        return categories;
    }
    
    /**
     * @brief 创建新文章并保存到文件
     * @param title 标题
     * @param author 作者
     * @param category 分类
     * @param content 内容
     * @return 创建的文章ID，失败返回空字符串
     */
    std::string createPost(const std::string& title, const std::string& author,
                         const std::string& category, const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            BlogPost post = BlogPost::createNew(title, author, category, content);
            std::string id = post.getId();
            
            // 确保文章目录存在
            if (!fs::exists(postsDirectory_)) {
                fs::create_directories(postsDirectory_);
            }
            
            // 保存文章到文件
            std::string filePath = (fs::path(postsDirectory_) / (id + ".md")).string();
            if (post.saveToFile(filePath)) {
                // 添加到内存中
                posts_.push_back(post);
                postsById_[id] = posts_.size() - 1;
                postsByCategory_[category].push_back(posts_.size() - 1);
                
                return id;
            }
        } catch (const std::exception& /* e */) {
            // 出错处理
        }
        
        return "";
    }
    
    /**
     * @brief 更新现有文章
     * @param id 文章ID
     * @param title 新标题
     * @param author 新作者
     * @param category 新分类
     * @param content 新内容
     * @return 是否更新成功
     */
    bool updatePost(const std::string& id, const std::string& title, 
                   const std::string& author, const std::string& category,
                   const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = postsById_.find(id);
        if (it == postsById_.end() || it->second >= posts_.size()) {
            return false;
        }
        
        try {
            // 获取旧文章
            BlogPost& post = posts_[it->second];
            std::string oldCategory = post.getCategory();
            
            // 创建临时新文章
            BlogPost updatedPost = BlogPost::createNew(title, author, category, content);
            updatedPost = BlogPost::createNew(title, author, category, content);
            
            // 保存到文件
            std::string filePath = (fs::path(postsDirectory_) / (id + ".md")).string();
            if (!updatedPost.saveToFile(filePath)) {
                return false;
            }
            
            // 更新内存中的文章
            post = updatedPost;
            
            // 如果分类变了，更新分类索引
            if (oldCategory != category) {
                rebuildIndices();
            }
            
            return true;
        } catch (const std::exception& /* e */) {
            // 出错处理
            return false;
        }
    }
    
    /**
     * @brief 删除文章
     * @param id 文章ID
     * @return 是否删除成功
     */
    bool deletePost(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = postsById_.find(id);
        if (it == postsById_.end() || it->second >= posts_.size()) {
            return false;
        }
        
        try {
            // 获取文章
            const BlogPost& post = posts_[it->second];
            
            // 删除文件
            std::string filePath = (fs::path(postsDirectory_) / (id + ".md")).string();
            if (fs::exists(filePath)) {
                fs::remove(filePath);
            }
            
            // 从内存中删除
            posts_.erase(posts_.begin() + it->second);
            
            // 重建索引
            rebuildIndices();
            
            return true;
        } catch (const std::exception& /* e */) {
            // 出错处理
            return false;
        }
    }
    
    /**
     * @brief 检查是否已加载
     * @return 是否已加载
     */
    bool isLoaded() const {
        return loaded_;
    }
    
    /**
     * @brief 获取文章数量
     * @return 文章数量
     */
    size_t getPostCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return posts_.size();
    }
    
private:
    // 单例模式：禁止外部构造和复制
    BlogPostManager() : loaded_(false) {}
    BlogPostManager(const BlogPostManager&) = delete;
    BlogPostManager& operator=(const BlogPostManager&) = delete;
    
    /**
     * @brief 重建所有索引
     */
    void rebuildIndices() {
        postsById_.clear();
        postsByCategory_.clear();
        
        for (size_t i = 0; i < posts_.size(); ++i) {
            postsById_[posts_[i].getId()] = i;
            postsByCategory_[posts_[i].getCategory()].push_back(i);
        }
    }
    
    std::string postsDirectory_;
    std::vector<BlogPost> posts_;
    std::unordered_map<std::string, size_t> postsById_;
    std::unordered_map<std::string, std::vector<size_t>> postsByCategory_;
    bool loaded_;
    mutable std::mutex mutex_;
};

#endif // TINA_BLOG_BLOG_POST_MANAGER_HPP 