//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_PLATFORM_HPP
#define TINA_BLOG_PLATFORM_HPP

// ==================== Nginx核心头文件 ====================
#ifdef _WIN32

// 禁用警告
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS

// 包含平台相关的头文件
extern "C" {
#include <ngx_win32_config.h>
}

#if (defined _WIN32 && defined _OFF_T_DEFINED)
typedef off_t _off_t;
#endif

#endif // _WIN32

#endif // TINA_BLOG_PLATFORM_HPP 
