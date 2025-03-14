//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_NGX_LIST_HPP
#define TINA_BLOG_NGX_LIST_HPP

#include "Nginx.hpp"
#include "NginxContext.hpp"
#include "NgxPool.hpp"
#include <type_traits>
#include <functional>
#include <iterator>
#include <cstddef>

/**
 * @brief Nginx链表封装类
 * 
 * 提供对ngx_list_t的高效封装，简化Nginx链表操作
 * @tparam T 链表元素类型
 */
template<typename T>
class NgxList : public NginxContext<ngx_list_t> {
public:
    /**
     * @brief 从已存在的ngx_list_t构造
     * @param list 链表指针
     * @param owns 是否拥有所有权
     */
    explicit NgxList(ngx_list_t* list, bool owns = false) noexcept
        : NginxContext<ngx_list_t>(list, owns)
    {
        // 确保T类型大小与链表元素大小一致
        static_assert(sizeof(T) <= 65535, "元素类型太大");
    }
    
    /**
     * @brief 在内存池中创建一个新的链表
     * @param pool 内存池
     * @param n 每个数组块中的元素数量
     * @param size 元素大小(如果与T不同，将发出警告)
     */
    explicit NgxList(const NgxPool& pool, ngx_uint_t n = 10, size_t size = sizeof(T)) noexcept
        : NginxContext<ngx_list_t>(ngx_list_create(pool.get(), n, size), true)
    {
        // 验证元素大小是否匹配
        if (size != sizeof(T)) {
            // 元素大小不匹配时记录警告
            ngx_log_error(NGX_LOG_WARN, pool.get()->log, 0, 
                          "NgxList element size mismatch: sizeof(T)=%zu, specified=%zu", 
                          sizeof(T), size);
        }
    }

    /**
     * @brief 创建一个无效链表
     */
    NgxList() noexcept : NginxContext<ngx_list_t>(nullptr, false) {}

    /**
     * @brief 添加新元素
     * @return 新元素的指针, 失败返回nullptr
     */
    T* add() const noexcept {
        if (!valid()) {
            return nullptr;
        }
        void* item = ngx_list_push(ptr_);
        return static_cast<T*>(item);
    }

    /**
     * @brief 添加新元素并初始化数据
     * @param data 要复制的数据
     * @return 新元素的指针, 失败返回nullptr
     */
    T* add(const T& data) const noexcept {
        T* item = add();
        if (item) {
            *item = data; // 复制数据
        }
        return item;
    }

    /**
     * @brief 初始化一个已分配的链表
     * @param pool 内存池
     * @param n 每个数组块中的元素数量
     * @param size 元素大小
     * @return 是否初始化成功
     */
    bool init(const NgxPool& pool, ngx_uint_t n = 10, size_t size = sizeof(T)) noexcept {
        if (!valid() || !pool.valid()) {
            return false;
        }
        return ngx_list_init(ptr_, pool.get(), n, size) == NGX_OK;
    }

    // 迭代器类，用于遍历链表
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        // 修改构造函数，接收整个链表或者其大小
        iterator(ngx_list_part_t* part, ngx_uint_t i, size_t size) 
            : part_(part), i_(i), size_(size) {}

        reference operator*() const {
            return *get_current();
        }

        pointer operator->() const {
            return get_current();
        }

        iterator& operator++() {
            if (!part_) return *this;
            
            ++i_;
            if (i_ >= part_->nelts) {
                if (part_->next) {
                    part_ = part_->next;
                    i_ = 0;
                } else {
                    // 到达链表末尾
                    part_ = nullptr;
                    i_ = 0;
                }
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return part_ == other.part_ && i_ == other.i_;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

    private:
        pointer get_current() const {
            if (!part_ || i_ >= part_->nelts) {
                // 安全检查：返回静态空对象
                static T dummy{}; // 静态空对象，仅用于防止解引用nullptr
                return &dummy;
            }
            
            // 计算当前元素的内存位置
            char* element_ptr = static_cast<char*>(part_->elts) + i_ * size_;
            return static_cast<pointer>(static_cast<void*>(element_ptr));
        }

        ngx_list_part_t* part_;
        ngx_uint_t i_;
        size_t size_; // 存储元素大小，从ngx_list_t获取
    };

    // 常量迭代器
    using const_iterator = const iterator;

    /**
     * @brief 获取指向链表开头的迭代器
     * @return 开始迭代器
     */
    iterator begin() const noexcept {
        if (!valid()) {
            return end();
        }
        return iterator(&ptr_->part, 0, ptr_->size);
    }

    /**
     * @brief 获取指向链表结尾的迭代器
     * @return 结束迭代器
     */
    iterator end() const noexcept {
        return iterator(nullptr, 0, 0);
    }

    /**
     * @brief 查找满足条件的元素
     * @param predicate 判断元素是否满足条件的函数
     * @return 找到的元素指针，如果没找到则返回nullptr
     */
    T* find_if(std::function<bool(const T&)> predicate) const noexcept {
        if (!valid()) {
            return nullptr;
        }

        // 遍历链表中的所有元素
        for (auto it = begin(); it != end(); ++it) {
            if (predicate(*it)) {
                return &(*it);
            }
        }
        return nullptr;
    }

    /**
     * @brief 链表是否为空
     * @return 链表为空返回true
     */
    [[nodiscard]] bool empty() const noexcept {
        if (!valid()) {
            return true;
        }
        return ptr_->part.nelts == 0 && ptr_->part.next == nullptr;
    }

    /**
     * @brief 获取链表中的元素数量
     * @return 元素数量
     */
    [[nodiscard]] size_t size() const noexcept {
        if (!valid()) {
            return 0;
        }
        
        size_t count = 0;
        for (ngx_list_part_t* part = &ptr_->part; part; part = part->next) {
            count += part->nelts;
        }
        return count;
    }

    /**
     * @brief 遍历链表中的所有元素
     * @param callback 处理每个元素的回调函数
     */
    void for_each(std::function<void(T&)> callback) const noexcept {
        if (!valid()) {
            return;
        }

        for (auto it = begin(); it != end(); ++it) {
            callback(*it);
        }
    }

    /**
     * @brief 重置链表，清除所有元素
     * 注意：这个操作只重置了链表的指针，实际内存并未释放
     */
    void reset() const noexcept {
        if (!valid()) {
            return;
        }
        
        ptr_->last = &ptr_->part;
        ptr_->part.nelts = 0;
        ptr_->part.next = nullptr;
    }

private:
    /**
     * @brief 清理资源
     * 如果拥有链表，则释放内存
     */
    void cleanup_impl() noexcept {
        if (ptr_ && owns_ptr_) {
            // ngx_list_t在内存池中分配，由内存池负责释放
            ptr_ = nullptr;
            owns_ptr_ = false;
        }
    }
};

#endif // TINA_BLOG_NGX_LIST_HPP
