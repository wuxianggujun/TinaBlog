# Nginx 模块开发指南

本文档详细记录了如何在 TinaBlog 项目中添加和开发自定义 Nginx 模块。

## 目录

- [1. 模块添加步骤](#1-模块添加步骤)
- [2. 模块结构设计](#2-模块结构设计)
- [3. 注册方式](#3-注册方式)
- [4. 配置处理](#4-配置处理)
- [5. 请求处理](#5-请求处理)
- [6. 常见问题](#6-常见问题)

## 1. 模块添加步骤

### 1.1 创建模块文件

在 `src/blog` 目录下，我们创建了以下文件来实现博客模块：

- **Nginx.hpp/cpp**: 基础包含文件，引入 Nginx 核心头文件
- **Platform.hpp**: 平台相关定义
- **ModuleRegistrar.hpp/cpp**: 模块注册器
- **HttpConfigHelper.hpp/cpp**: HTTP 配置助手
- **BlogModule.hpp/cpp**: 博客模块实现

### 1.2 修改 ngx_modules.c

在项目根目录的 `ngx_modules.c` 文件中添加模块声明和引用：

```c
// 添加外部模块声明
extern ngx_module_t ngx_http_blog_module;

// 在 ngx_modules 数组中添加
ngx_module_t *ngx_modules[] = {
    &ngx_core_module,
    &ngx_errlog_module,
    // ... 其他模块
    
    // 添加博客模块
    &ngx_http_blog_module,
    
    NULL
};

// 在 ngx_module_names 数组中添加
char *ngx_module_names[] = {
    "ngx_core_module",
    "ngx_errlog_module",
    // ... 其他模块名
    
    // 添加博客模块名
    "ngx_http_blog_module",
    
    NULL
};
```

### 1.3 更新 CMakeLists.txt

在 `src/CMakeLists.txt` 文件中添加我们的模块源文件：

```cmake
# Blog模块
blog/Nginx.cpp
blog/ModuleRegistrar.cpp
blog/HttpConfigHelper.cpp
blog/BlogModule.cpp
```

### 1.4 配置 Nginx

在 Nginx 配置文件 (nginx.conf) 中使用我们的自定义指令：

```nginx
location / {
    # 博客模块配置
    blog_path /path/to/blog;
    blog_template_path /path/to/templates;
    blog_enable_cache on;
    blog_cache_time 60;
}
```

## 2. 模块结构设计

### 2.1 模块定义

在 `BlogModule.cpp` 中，我们使用以下结构定义模块：

```cpp
// 命令定义
static ngx_command_t blog_commands[] = {
    { ngx_string("blog_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      BlogModule::setBlogPath,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(BlogModuleConfig, base_path),
      NULL },
    // ... 其他命令
    ngx_null_command
};

// 模块上下文
static ngx_http_module_t blog_module_ctx = {
    BlogModule::preConfiguration,   /* preconfiguration */
    BlogModule::postConfiguration,  /* postconfiguration */
    // ... 其他回调
};

// 全局模块结构
extern "C" {
    ngx_module_t ngx_http_blog_module = {
        NGX_MODULE_V1,
        &blog_module_ctx,     /* module context */
        blog_commands,        /* module directives */
        NGX_HTTP_MODULE,      /* module type */
        NULL,                 /* init master */
        NULL,                 /* init module */
        NULL,                 /* init process */
        NULL,                 /* init thread */
        NULL,                 /* exit thread */
        NULL,                 /* exit process */
        NULL,                 /* exit master */
        NGX_MODULE_V1_PADDING
    };
}
```

### 2.2 配置结构

定义在 `BlogModule.hpp` 中：

```cpp
struct BlogModuleConfig {
    ngx_str_t base_path;    // 博客基础路径
    ngx_str_t template_path; // 模板路径
    ngx_flag_t enable_cache; // 是否启用缓存
    ngx_uint_t cache_time;   // 缓存时间（秒）
};
```

## 3. 注册方式

我们使用两种注册方法：

### 3.1 直接集成方法 (推荐)

直接在 `ngx_modules.c` 中添加模块引用，在 Nginx 启动时自动加载。

### 3.2 动态注册方法 (备选)

通过 `ModuleRegistrar` 类在运行时注册模块：

```cpp
bool BlogModule::registerModule() {
    // 创建模块上下文和命令
    // 注册到模块注册器
    return ModuleRegistrar::instance().registerHttpModule(module);
}
```

## 4. 配置处理

### 4.1 创建配置

```cpp
void* BlogModule::createLocationConfig(ngx_conf_t* cf) {
    auto* conf = static_cast<BlogModuleConfig*>(
        ngx_pcalloc(cf->pool, sizeof(BlogModuleConfig)));
    
    if (conf == nullptr) {
        return nullptr;
    }
    
    // 设置默认值
    conf->enable_cache = NGX_CONF_UNSET;
    conf->cache_time = NGX_CONF_UNSET_UINT;
    
    return conf;
}
```

### 4.2 合并配置

```cpp
char* BlogModule::mergeLocationConfig(ngx_conf_t* cf, void* parent, void* child) {
    auto* prev = static_cast<BlogModuleConfig*>(parent);
    auto* conf = static_cast<BlogModuleConfig*>(child);
    
    // 合并配置
    ngx_conf_merge_str_value(conf->base_path, prev->base_path, "");
    ngx_conf_merge_str_value(conf->template_path, prev->template_path, "");
    ngx_conf_merge_value(conf->enable_cache, prev->enable_cache, 0);
    ngx_conf_merge_uint_value(conf->cache_time, prev->cache_time, 60);
    
    return NGX_CONF_OK;
}
```

### 4.3 指令处理

```cpp
char* BlogModule::setBlogPath(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    ngx_str_t* value = static_cast<ngx_str_t*>(cf->args->elts);
    
    // 设置博客路径
    bmconf->base_path = value[1];
    
    return NGX_CONF_OK;
}
```

## 5. 请求处理

### 5.1 注册处理函数

在 `postConfiguration` 方法中注册请求处理函数：

```cpp
ngx_int_t BlogModule::postConfiguration(ngx_conf_t* cf) {
    ngx_http_core_main_conf_t* cmcf;
    ngx_http_handler_pt* h;
    
    // 获取HTTP核心模块的主配置
    cmcf = static_cast<ngx_http_core_main_conf_t*>(
        ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module));
    
    // 添加处理器到内容阶段
    h = static_cast<ngx_http_handler_pt*>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers));
    
    if (h == nullptr) {
        return NGX_ERROR;
    }
    
    // 设置处理器
    *h = handleBlogRequest;
    
    return NGX_OK;
}
```

### 5.2 请求处理函数

```cpp
ngx_int_t BlogModule::handleBlogRequest(ngx_http_request_t* r) {
    // 获取位置配置
    auto* conf = static_cast<BlogModuleConfig*>(
        ngx_http_get_module_loc_conf(r, ngx_http_core_module));
    
    // 处理请求
    // ...
    
    // 发送响应
    return ngx_http_output_filter(r, &out);
}
```

## 6. 常见问题

### 6.1 访问权限问题

确保回调函数是公开的 (public)，否则会导致编译错误：

```cpp
class BlogModule {
public:
    // 必须为public
    static ngx_int_t preConfiguration(ngx_conf_t* cf);
    static ngx_int_t postConfiguration(ngx_conf_t* cf);
    static void* createMainConfig(ngx_conf_t* cf);
    // ... 其他回调
};
```

### 6.2 类型转换警告

使用 ngx_string 或 ngx_str_set 宏来处理字符串：

```cpp
// 不好的做法:
if (ngx_strcasecmp(value[1].data, reinterpret_cast<u_char*>("on")) == 0) { ... }

// 好的做法:
ngx_str_t on_str = ngx_string("on");
if (ngx_strcasecmp(value[1].data, on_str.data) == 0) { ... }
```

### 6.3 配置文件错误

确保配置文件中的花括号匹配：

```nginx
http {
    server {
        location / {
            # 配置
        } # 每个左花括号需要有匹配的右花括号
    }
}
``` 