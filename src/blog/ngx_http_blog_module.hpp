//
// Created by wuxianggujun on 2025/3/11.
//

#ifndef NGX_HTTP_BLOG_MODULE_HPP
#define NGX_HTTP_BLOG_MODULE_HPP

// 包含Nginx核心头文件
#include "Nginx.hpp"

extern "C" {
// 声明Nginx模块
extern ngx_module_t ngx_http_blog_module;
}

// 博客模块的主配置结构
struct ngx_http_blog_main_conf_t
{
    ngx_str_t blog_root;       // 博客根目录
    ngx_str_t database_path;   // 数据库文件路径
    ngx_uint_t cache_time;     // 缓存时间(秒)
    ngx_flag_t enable_comment; // 是否启用评论
};

// 博客模块的定位配置结构
struct ngx_http_blog_loc_conf_t
{
    ngx_flag_t enable;         // 该位置是否启用博客
    ngx_str_t template_path;   // 模板文件路径
};

// 博客文章元数据结构 - 暂时注释掉，以后再实现
/*
struct blog_post_t
{
    ngx_str_t id;              // 文章ID
    ngx_str_t title;           // 文章标题
    ngx_str_t author;          // 作者
    ngx_str_t content;         // 内容
    ngx_str_t summary;         // 摘要
    time_t publish_time;       // 发布时间
    time_t update_time;        // 更新时间
    ngx_array_t* tags;         // 标签数组
    ngx_uint_t view_count;     // 浏览次数
    ngx_flag_t is_published;   // 是否已发布
};
*/

#endif //NGX_HTTP_BLOG_MODULE_HPP
