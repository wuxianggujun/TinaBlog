//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_HPP
#define TINA_BLOG_NGINX_HPP

extern "C" {
#include <ngx_http.h>
}

#include <cassert>
// 用于忽略某些不使用的变量
#include <boost/core/ignore_unused.hpp>

class Nginx
{
};


#endif //TINA_BLOG_NGINX_HPP
