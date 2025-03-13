//
// Created by wuxianggujun on 2025/3/13.
//

#ifndef TINA_BLOG_NGINX_MODULE_LOADER_HPP
#define TINA_BLOG_NGINX_MODULE_LOADER_HPP

#include "Nginx.hpp"
#include "NginxLog.hpp"

/**
 * NginxModuleLoader 类用于简化Nginx模块的加载过程
 * 
 * 注意：此类不能实际自动加载模块，因为Nginx要求动态模块必须通过load_module指令加载
 * 它主要用于提供模块加载的相关信息和帮助函数
 */
class NginxModuleLoader
{
public:
    /**
     * 获取模块加载器的单例实例
     */
    static NginxModuleLoader& instance()
    {
        static NginxModuleLoader loader;
        return loader;
    }
    
    /**
     * 注册模块 - 在模块初始化时调用
     * 
     * @param module_name 模块名称
     * @param module 模块指针
     */
    void registerModule(const char* module_name, ngx_module_t* module)
    {
        if (m_initialized) {
            return;
        }
        
        m_module_name = module_name;
        m_module = module;
        m_initialized = true;
        
        NginxStderrLog().print("Module %s registered", module_name);
    }
    
    /**
     * 获取模块配置指令的用法信息
     */
    const char* getUsageInfo() const
    {
        return "To use this module, add the following to your nginx.conf:\n"
               "load_module modules/ngx_http_blog_module.so;\n\n"
               "http {\n"
               "    blog_root /path/to/blog;\n"
               "    blog_db_path /path/to/blog/data/blog.db;\n"
               "    server {\n"
               "        location /blog {\n"
               "            blog_enable on;\n"
               "        }\n"
               "    }\n"
               "}\n";
    }
    
    /**
     * 检查模块是否正确加载
     */
    bool isModuleLoaded() const
    {
        return m_initialized;
    }
    
    /**
     * 获取模块名称
     */
    const char* getModuleName() const
    {
        return m_module_name;
    }
    
private:
    // 私有构造函数 - 单例模式
    NginxModuleLoader() : m_initialized(false), m_module(nullptr), m_module_name(nullptr) {}
    
    // 禁止复制和赋值
    NginxModuleLoader(const NginxModuleLoader&) = delete;
    NginxModuleLoader& operator=(const NginxModuleLoader&) = delete;
    
    bool m_initialized;           // 模块是否已初始化
    ngx_module_t* m_module;       // 模块指针
    const char* m_module_name;    // 模块名称
};

#endif //TINA_BLOG_NGINX_MODULE_LOADER_HPP 