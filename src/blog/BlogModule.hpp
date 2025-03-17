//
// Created by wuxianggujun on 2025/3/14.
//

#ifndef TINA_BLOG_MODULE_HPP
#define TINA_BLOG_MODULE_HPP

#include "Nginx.hpp"
#include "BlogConfig.hpp"
#include "BlogRouter.hpp"
#include <string>
#include <unordered_map>

// 前向声明
class NgxResponse;

/**
 * @brief 博客模块类
 * 
 * 实现Nginx博客模块的核心功能，管理模块配置、路由和初始化
 */
class BlogModule {
public:
    //=====================================================================
    // 核心模块方法
    //=====================================================================
    
    /**
     * @brief 初始化模块
     * @param cycle Nginx周期对象
     * @return NGX_OK成功，NGX_ERROR失败
     */
    static ngx_int_t init(ngx_cycle_t* cycle);
    
    /**
     * @brief HTTP请求处理函数
     * @param r Nginx请求对象
     * @return Nginx状态码
     */
    static ngx_int_t handleRequest(ngx_http_request_t* r);
    
    /**
     * @brief 获取模块名称
     * @return 模块名称
     */
    static const char* getModuleName();
    
    /**
     * @brief 获取模块版本
     * @return 模块版本
     */
    static const char* getModuleVersion();
    
    /**
     * @brief 获取静态资源路径
     * @return 静态资源路径
     */
    static std::string getStaticPath();
    
    //=====================================================================
    // 路由处理函数
    //=====================================================================
    
    /**
     * @brief 初始化博客路由
     * @param router 路由器指针
     */
    static void initBlogRoutes(BlogRouter* router);
    
    /**
     * @brief 处理静态文件请求
     * @param r Nginx请求对象
     * @return Nginx状态码
     */
    static ngx_int_t handleStaticRequest(ngx_http_request_t* r);
    
    /**
     * @brief 提供静态文件
     * @param r Nginx请求对象
     * @param path 文件路径
     * @return Nginx状态码
     */
    static ngx_int_t serveStaticFile(ngx_http_request_t* r, const std::string& path);
    
    /**
     * @brief 处理博客首页API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleBlogIndex(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理博客文章详情API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleBlogPost(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理分类文章列表API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleBlogCategory(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理标签文章列表API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleBlogTag(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理管理面板API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleAdmin(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理添加文章API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleAddPost(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理编辑文章API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleEditPost(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理删除文章API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleDeletePost(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理博客重定向
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleBlogRedirect(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理管理面板统计API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleAdminStats(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理获取文章编辑数据API
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleGetPostForEdit(NgxResponse& response, const RouteParams& params);
    
    /**
     * @brief 处理OPTIONS请求
     * @param response Nginx响应封装
     * @param params 路由参数
     * @return Nginx状态码
     */
    static ngx_int_t handleOptionsRequest(NgxResponse& response, const RouteParams& params);
    
    //=====================================================================
    // Nginx配置回调方法
    //=====================================================================
    
    /**
     * @brief 预配置阶段回调
     * @param cf Nginx配置对象
     * @return NGX_OK成功，NGX_ERROR失败
     */
    static ngx_int_t preConfiguration(ngx_conf_t* cf);
    
    /**
     * @brief 后配置阶段回调
     * @param cf Nginx配置对象
     * @return NGX_OK成功，NGX_ERROR失败
     */
    static ngx_int_t postConfiguration(ngx_conf_t* cf);
    
    /**
     * @brief 创建主配置
     * @param cf Nginx配置对象
     * @return 配置对象指针
     */
    static void* createMainConfig(ngx_conf_t* cf);
    
    /**
     * @brief 初始化主配置
     * @param cf Nginx配置对象
     * @param conf 配置对象指针
     * @return NGX_CONF_OK成功，其他失败
     */
    static char* initMainConfig(ngx_conf_t* cf, void* conf);
    
    /**
     * @brief 创建服务器配置
     * @param cf Nginx配置对象
     * @return 配置对象指针
     */
    static void* createServerConfig(ngx_conf_t* cf);
    
    /**
     * @brief 合并服务器配置
     * @param cf Nginx配置对象
     * @param parent 父配置
     * @param child 子配置
     * @return NGX_CONF_OK成功，其他失败
     */
    static char* mergeServerConfig(ngx_conf_t* cf, void* parent, void* child);
    
    /**
     * @brief 创建位置配置
     * @param cf Nginx配置对象
     * @return 配置对象指针
     */
    static void* createLocationConfig(ngx_conf_t* cf);
    
    /**
     * @brief 合并位置配置
     * @param cf Nginx配置对象
     * @param parent 父配置
     * @param child 子配置
     * @return NGX_CONF_OK成功，其他失败
     */
    static char* mergeLocationConfig(ngx_conf_t* cf, void* parent, void* child);
    
    //=====================================================================
    // 配置指令处理方法
    //=====================================================================
    
    /**
     * @brief 设置博客路径
     * @param cf Nginx配置对象
     * @param cmd 命令对象
     * @param conf 配置对象指针
     * @return NGX_CONF_OK成功，其他失败
     */
    static char* setBlogPath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 设置启用缓存
     * @param cf Nginx配置对象
     * @param cmd 命令对象
     * @param conf 配置对象指针
     * @return NGX_CONF_OK成功，其他失败
     */
    static char* setEnableCache(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 设置缓存时间
     * @param cf Nginx配置对象
     * @param cmd 命令对象
     * @param conf 配置对象指针
     * @return NGX_CONF_OK成功，其他失败
     */
    static char* setCacheTime(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 设置数据库连接字符串
     * @param cf Nginx配置对象
     * @param cmd 命令对象
     * @param conf 配置对象指针
     * @return NGX_CONF_OK成功，其他失败
     */
    static char* setDbConnectionString(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    /**
     * @brief 设置自动连接数据库
     * @param cf Nginx配置对象
     * @param cmd 命令对象
     * @param conf 配置对象指针
     * @return NGX_CONF_OK成功，其他失败
     */
    static char* setDbAutoConnect(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    
    //=====================================================================
    // 模块数据
    //=====================================================================
    
    /// 模块是否已注册标志
    static bool isRegistered_;
    
    /// 模块对象指针
    static ngx_module_t* blogModule_;
    
    /// 模块基础路径
    static std::string basePath;
    
    /// 模块版本
    static std::string version;
    
    /// 数据目录
    static std::string dataDir;
    
    /// 静态资源路径
    static std::string staticPath;
};

#endif // TINA_BLOG_MODULE_HPP 