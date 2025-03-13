//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_HPP
#define TINA_BLOG_NGINX_HPP

// 2. 解决off_t类型问题
#include <io.h>
#if (defined _WIN32 && defined _OFF_T_DEFINED)
typedef off_t _off_t;
#endif


// ========= Nginx核心头文件 =========
// 使用C链接约定包含Nginx头文件
extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
}

// ========= C++标准库 =========
#include <cassert>
#include <ctime>
#include <string>
#include <memory>
#include <stdexcept>

// ========= Boost库 =========
// 用于忽略某些不使用的变量
#include <boost/core/ignore_unused.hpp>

#endif //TINA_BLOG_NGINX_HPP
