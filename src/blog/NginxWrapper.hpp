//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_WRAPPER_HPP
#define TINA_BLOG_NGINX_WRAPPER_HPP

#include <boost/type_traits.hpp>

/**
 * NginxWrapper 只能被继承，是一个抽象类
 */
template <typename T>
class NginxWrapper
{
public:
    typedef boost::remove_pointer_t<T> wrapped_type;
    typedef wrapped_type* pointer_type;
    typedef wrapped_type& reference_type;

    pointer_type get() const { return m_ptr; }

    // 转型为bool类型
    explicit operator bool() const { return get(); }

    // 转型为指针类型
    explicit operator pointer_type() const { return get(); }
    // 指针操作符重载
    pointer_type operator->() const { return get(); }

protected:
    // 参数是指针类型
    explicit NginxWrapper(pointer_type ptr) : m_ptr(ptr)
    {
    }

    // 参数是引用类型
    explicit NginxWrapper(reference_type ref) : m_ptr(&ref)
    {
    }

private:
    // 使用指针保存对象，默认是空指针
    pointer_type m_ptr = nullptr;
};


#endif //TINA_BLOG_NGINX_WRAPPER_HPP
