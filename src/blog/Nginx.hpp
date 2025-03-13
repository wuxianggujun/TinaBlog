//
// Created by wuxianggujun on 2025/3/12.
//

#ifndef TINA_BLOG_NGINX_HPP
#define TINA_BLOG_NGINX_HPP

#include "Platform.hpp"

// ==================== Nginx核心头文件 ====================
extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
}

// ==================== C++标准库 ====================
#include <cassert>
#include <ctime>
#include <string>
#include <memory>
#include <stdexcept>

// ==================== Boost库 ====================
#include <boost/core/ignore_unused.hpp>

#endif //TINA_BLOG_NGINX_HPP
