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
#include <ngx_event.h>
#include <ngx_event_connect.h>
#include <ngx_connection.h>
#include <ngx_thread_pool.h>
#include <ngx_thread.h>
#include <ngx_resolver.h>
#include <ngx_log.h>
#include <ngx_palloc.h>
#include <ngx_array.h>
#include <ngx_string.h>
#include <ngx_hash.h>
#include <ngx_queue.h>
#include <ngx_file.h>
}

// ==================== C++标准库 ====================
#include <cassert>
#include <ctime>
#include <string>
#include <memory>
#include <stdexcept>


#endif //TINA_BLOG_NGINX_HPP
