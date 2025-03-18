#pragma once

#include "Nginx.hpp"

extern "C"{
    // 声明模块变量
    extern ngx_module_t ngx_http_blog_module;
}

// 模块配置结构
struct BlogModuleConfig {
    ngx_str_t base_path;            // 博客内容基础路径
    ngx_flag_t enable_cache;        // 是否启用缓存
    ngx_uint_t cache_time;          // 缓存时间（秒）
    ngx_str_t db_connection;        // 数据库连接字符串
    ngx_flag_t db_auto_connect;     // 是否自动连接数据库
};

class BlogModule {
public:
    // 配置相关函数
    static void* createLocConf(ngx_conf_t* cf);      // 创建配置结构
    static char* mergeLocConf(ngx_conf_t* cf, void* parent, void* child);  // 合并配置
    
    // 配置指令处理函数
    static char* setBlogPath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);         // 设置博客路径
    static char* setEnableCache(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);      // 设置是否启用缓存
    static char* setCacheTime(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);        // 设置缓存时间
    static char* setDbConnection(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);     // 设置数据库连接
    static char* setDbAutoConnect(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);    // 设置是否自动连接
    
    // 模块生命周期函数
    static ngx_int_t preConfiguration(ngx_conf_t* cf);       // 预配置
    static ngx_int_t postConfiguration(ngx_conf_t* cf);      // 后配置
    static ngx_int_t initProcess(ngx_cycle_t* cycle);        // 进程初始化
    static void exitProcess(ngx_cycle_t* cycle);             // 进程退出
};


