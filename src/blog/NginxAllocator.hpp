//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_ALLOCATOR_HPP
#define TINA_BLOG_NGINX_ALLOCATOR_HPP

#include <NginxWrapper.hpp>

template <typename T>
class NginxAllocator : public NginxWrapper<ngx_pool_t>
{
public:
    typedef NginxWrapper<ngx_pool_t> super_type;
    typedef NginxAllocator this_type;

    typedef std::size_t size_type;
    typedef T* pointer;

    typedef T value_type;

    explicit NginxAllocator(ngx_pool_t* p): super_type(p)
    {
    }

    ~NginxAllocator() = default;

    // 分配n个元素所需的内存
    pointer allocate(size_type n)
    {
        return reinterpret_cast<pointer>(ngx_pnalloc(get(),n* sizeof(T)));
    }

    void deallocate(pointer ptr, size_type n)
    {
        // 忽略入口函数
        boost::ignore_unused(n);
        // 释放内存
        ngx_pfree(get(),ptr);
    }
    
};


#endif //TINA_BLOG_NGINX_ALLOCATOR_HPP
