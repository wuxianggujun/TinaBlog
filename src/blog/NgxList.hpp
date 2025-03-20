#pragma once
#include "NgxPtr.hpp"
#include <type_traits>
#include <vector>
#include <optional>
#include <functional>

/**
 * @brief Nginx链表包装类
 * 
 * @tparam T 链表元素类型
 */
template<typename T>
class NgxList : public NgxPtr<ngx_list_t> {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = ngx_uint_t;
    
    // 使用基类的构造函数
    using NgxPtr<ngx_list_t>::NgxPtr;
    
    // 获取元素数量 (注意：这是O(n)操作)
    [[nodiscard]] size_type size() const noexcept {
        if (!get()) return 0;
        
        size_type count = 0;
        const ngx_list_part_t* part = &get()->part;
        
        while (part) {
            count += part->nelts;
            part = part->next;
        }
        
        return count;
    }
    
    // 是否为空
    [[nodiscard]] bool empty() const noexcept {
        return get() ? (get()->part.nelts == 0 && get()->part.next == nullptr) : true;
    }
    
    // 添加元素
    pointer push() {
        if (!ptr_) return nullptr;
        return static_cast<pointer>(ngx_list_push(ptr_));
    }
    
    // 遍历列表中的所有元素 (使用回调函数)
    template<typename Func>
    void for_each(Func&& func) const {
        if (!get()) return;
        
        const ngx_list_part_t* part = &get()->part;
        const_pointer data = static_cast<const_pointer>(part->elts);
        
        while (part) {
            for (ngx_uint_t i = 0; i < part->nelts; i++) {
                func(data[i]);
            }
            
            part = part->next;
            if (part) {
                data = static_cast<const_pointer>(part->elts);
            }
        }
    }
    
    // 查找第一个符合条件的元素
    template<typename Predicate>
    std::optional<std::reference_wrapper<const T>> find_if(Predicate&& pred) const {
        if (!get()) return std::nullopt;
        
        const ngx_list_part_t* part = &get()->part;
        const_pointer data = static_cast<const_pointer>(part->elts);
        
        while (part) {
            for (ngx_uint_t i = 0; i < part->nelts; i++) {
                if (pred(data[i])) {
                    return std::reference_wrapper<const T>(data[i]);
                }
            }
            
            part = part->next;
            if (part) {
                data = static_cast<const_pointer>(part->elts);
            }
        }
        
        return std::nullopt;
    }
    
    // 转换为vector (方便操作)
    std::vector<T> to_vector() const {
        std::vector<T> result;
        
        if (!get()) return result;
        
        // 预先分配空间
        result.reserve(size());
        
        for_each([&result](const T& item) {
            result.push_back(item);
        });
        
        return result;
    }
}; // class NgxList 