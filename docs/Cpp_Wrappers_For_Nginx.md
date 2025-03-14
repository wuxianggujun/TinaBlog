# Nginx C结构的C++封装

本文档提供了将Nginx原生C结构封装为现代C++对象的建议和实现方法。

## 目录

- [1. 封装原则](#1-封装原则)
- [2. ngx_str_t封装](#2-ngx_str_t封装)
- [3. ngx_conf_t封装](#3-ngx_conf_t封装)
- [4. 内存池封装](#4-内存池封装)
- [5. 请求对象封装](#5-请求对象封装)
- [6. 配置指令封装](#6-配置指令封装)
- [7. 完整示例](#7-完整示例)
- [8. Nginx基类设计](#8-nginx基类设计)

## 1. 封装原则

在封装Nginx C结构时，我们遵循以下原则：

1. **保持兼容性**：确保封装类能与原生C API无缝协作
2. **提供安全访问**：使用RAII（资源获取即初始化）管理生命周期
3. **简化接口**：提供更直观且类型安全的方法
4. **避免性能开销**：尽量减少不必要的复制和间接调用
5. **现代C++风格**：使用C++11及以上特性，如智能指针、移动语义等

## 2. ngx_str_t封装

`ngx_str_t`是Nginx中常用的字符串结构，我们可以为其创建一个C++封装类：

```cpp
class NgxString {
public:
    // 从ngx_str_t构造
    NgxString(const ngx_str_t& str)
        : data_(str.data), length_(str.len) {}
    
    // 从C++字符串构造
    NgxString(const std::string& str) {
        length_ = str.length();
        data_ = new u_char[length_];
        std::memcpy(data_, str.c_str(), length_);
    }
    
    // 移动构造函数
    NgxString(NgxString&& other) noexcept
        : data_(other.data_), length_(other.length_) {
        other.data_ = nullptr;
        other.length_ = 0;
    }
    
    // 析构函数
    ~NgxString() {
        if (data_ != nullptr && owns_memory_) {
            delete[] data_;
        }
    }
    
    // 转换回ngx_str_t
    ngx_str_t to_ngx_str() const {
        ngx_str_t result;
        result.data = data_;
        result.len = length_;
        return result;
    }
    
    // 转换为std::string
    std::string to_string() const {
        return std::string(reinterpret_cast<char*>(data_), length_);
    }
    
    // 比较操作
    bool equals(const NgxString& other) const {
        if (length_ != other.length_) {
            return false;
        }
        return std::memcmp(data_, other.data_, length_) == 0;
    }
    
    // 从Nginx宏创建
    static NgxString from_literal(const char* literal) {
        ngx_str_t temp = ngx_string(literal);
        return NgxString(temp);
    }
    
private:
    u_char* data_ = nullptr;
    size_t length_ = 0;
    bool owns_memory_ = true;  // 是否需要在析构时释放内存
};
```

## 3. ngx_conf_t封装

`ngx_conf_t`是Nginx配置上下文，它可以封装为以下C++类：

```cpp
class NgxConfContext {
public:
    // 从ngx_conf_t*构造
    NgxConfContext(ngx_conf_t* conf)
        : conf_(conf) {}
    
    // 获取参数数组
    std::vector<NgxString> get_args() const {
        std::vector<NgxString> result;
        ngx_str_t* values = static_cast<ngx_str_t*>(conf_->args->elts);
        
        for (ngx_uint_t i = 0; i < conf_->args->nelts; ++i) {
            result.emplace_back(values[i]);
        }
        
        return result;
    }
    
    // 获取特定参数
    NgxString get_arg(size_t index) const {
        if (index >= conf_->args->nelts) {
            throw std::out_of_range("Argument index out of range");
        }
        
        ngx_str_t* values = static_cast<ngx_str_t*>(conf_->args->elts);
        return NgxString(values[index]);
    }
    
    // 获取内存池
    ngx_pool_t* get_pool() const {
        return conf_->pool;
    }
    
    // 获取原始指针
    ngx_conf_t* get_raw() const {
        return conf_;
    }
    
    // 获取模块配置
    template<typename T>
    T* get_module_main_conf(ngx_module_t& module) const {
        return static_cast<T*>(
            ngx_http_conf_get_module_main_conf(conf_, module));
    }
    
    template<typename T>
    T* get_module_srv_conf(ngx_module_t& module) const {
        return static_cast<T*>(
            ngx_http_conf_get_module_srv_conf(conf_, module));
    }
    
    template<typename T>
    T* get_module_loc_conf(ngx_module_t& module) const {
        return static_cast<T*>(
            ngx_http_conf_get_module_loc_conf(conf_, module));
    }
    
private:
    ngx_conf_t* conf_ = nullptr;
};
```

## 4. 内存池封装

Nginx的内存池系统可以封装为更安全的C++类：

```cpp
class NgxPool {
public:
    // 创建新的内存池
    NgxPool(size_t size = NGX_DEFAULT_POOL_SIZE)
        : pool_(ngx_create_pool(size, nullptr)) {
        if (pool_ == nullptr) {
            throw std::bad_alloc();
        }
    }
    
    // 从现有池构造
    NgxPool(ngx_pool_t* pool)
        : pool_(pool), owns_pool_(false) {}
    
    // 析构函数
    ~NgxPool() {
        if (pool_ != nullptr && owns_pool_) {
            ngx_destroy_pool(pool_);
        }
    }
    
    // 分配内存
    template<typename T>
    T* alloc() {
        void* p = ngx_pcalloc(pool_, sizeof(T));
        if (p == nullptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(p);
    }
    
    // 分配数组
    template<typename T>
    T* alloc_array(size_t count) {
        void* p = ngx_pcalloc(pool_, sizeof(T) * count);
        if (p == nullptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(p);
    }
    
    // 分配字符串
    NgxString alloc_string(const std::string& str) {
        u_char* data = static_cast<u_char*>(
            ngx_pnalloc(pool_, str.length()));
        
        if (data == nullptr) {
            throw std::bad_alloc();
        }
        
        std::memcpy(data, str.c_str(), str.length());
        
        ngx_str_t ngx_str;
        ngx_str.data = data;
        ngx_str.len = str.length();
        
        return NgxString(ngx_str);
    }
    
    // 获取原始池指针
    ngx_pool_t* get_raw() const {
        return pool_;
    }
    
private:
    ngx_pool_t* pool_ = nullptr;
    bool owns_pool_ = true;  // 是否需要在析构时销毁池
};
```

## 5. 请求对象封装

`ngx_http_request_t`可以封装为更易用的C++类：

```cpp
class NgxHttpRequest {
public:
    // 从ngx_http_request_t*构造
    NgxHttpRequest(ngx_http_request_t* r)
        : request_(r) {}
    
    // 获取请求方法
    std::string get_method() const {
        if (request_->method_name.len > 0) {
            return std::string(
                reinterpret_cast<char*>(request_->method_name.data),
                request_->method_name.len);
        }
        
        // 如果没有方法名，返回方法ID
        switch (request_->method) {
            case NGX_HTTP_GET:
                return "GET";
            case NGX_HTTP_POST:
                return "POST";
            case NGX_HTTP_HEAD:
                return "HEAD";
            // ... 其他方法
            default:
                return "UNKNOWN";
        }
    }
    
    // 获取URI
    NgxString get_uri() const {
        return NgxString(request_->uri);
    }
    
    // 获取参数
    NgxString get_args() const {
        return NgxString(request_->args);
    }
    
    // 解析特定参数
    std::optional<std::string> get_arg(const std::string& name) const {
        ngx_str_t arg_name;
        arg_name.data = reinterpret_cast<u_char*>(const_cast<char*>(name.c_str()));
        arg_name.len = name.length();
        
        ngx_str_t arg_value;
        if (ngx_http_arg(request_, arg_name.data, arg_name.len, &arg_value) == NGX_OK) {
            return std::string(
                reinterpret_cast<char*>(arg_value.data),
                arg_value.len);
        }
        
        return std::nullopt;
    }
    
    // 获取模块配置
    template<typename T>
    T* get_module_main_conf(ngx_module_t& module) const {
        return static_cast<T*>(
            ngx_http_get_module_main_conf(request_, module));
    }
    
    template<typename T>
    T* get_module_srv_conf(ngx_module_t& module) const {
        return static_cast<T*>(
            ngx_http_get_module_srv_conf(request_, module));
    }
    
    template<typename T>
    T* get_module_loc_conf(ngx_module_t& module) const {
        return static_cast<T*>(
            ngx_http_get_module_loc_conf(request_, module));
    }
    
    // 发送响应
    ngx_int_t send_response(int status_code, 
                         const std::string& content_type,
                         const std::string& content) {
        // 设置状态码
        request_->headers_out.status = status_code;
        
        // 设置内容类型
        request_->headers_out.content_type.len = content_type.length();
        request_->headers_out.content_type.data = 
            reinterpret_cast<u_char*>(const_cast<char*>(content_type.c_str()));
        
        // 设置内容长度
        request_->headers_out.content_length_n = content.length();
        
        // 发送头部
        ngx_int_t rc = ngx_http_send_header(request_);
        if (rc == NGX_ERROR || rc > NGX_OK || request_->header_only) {
            return rc;
        }
        
        // 创建缓冲区
        NgxPool pool(request_->pool);
        ngx_buf_t* buffer = pool.alloc<ngx_buf_t>();
        buffer->pos = reinterpret_cast<u_char*>(const_cast<char*>(content.c_str()));
        buffer->last = buffer->pos + content.length();
        buffer->memory = 1;
        buffer->last_buf = 1;
        
        // 创建缓冲区链
        ngx_chain_t out;
        out.buf = buffer;
        out.next = nullptr;
        
        // 发送内容
        return ngx_http_output_filter(request_, &out);
    }
    
    // 获取原始请求指针
    ngx_http_request_t* get_raw() const {
        return request_;
    }
    
private:
    ngx_http_request_t* request_ = nullptr;
};
```

## 6. 配置指令封装

Nginx配置指令可以封装为更易于构建的C++类：

```cpp
class NgxCommand {
public:
    // 构造函数
    NgxCommand(const std::string& name,
              ngx_uint_t type,
              char* (*handler)(ngx_conf_t*, ngx_command_t*, void*),
              ngx_uint_t conf,
              size_t offset,
              void* post = nullptr)
    {
        command_.name.len = name.length();
        command_.name.data = reinterpret_cast<u_char*>(const_cast<char*>(name.c_str()));
        command_.type = type;
        command_.set = handler;
        command_.conf = conf;
        command_.offset = offset;
        command_.post = post;
    }
    
    // 从ngx_command_t构造
    NgxCommand(const ngx_command_t& cmd)
        : command_(cmd) {}
    
    // 获取原始命令
    ngx_command_t get_raw() const {
        return command_;
    }
    
    // 创建null命令
    static NgxCommand null_command() {
        ngx_command_t cmd;
        std::memset(&cmd, 0, sizeof(ngx_command_t));
        return NgxCommand(cmd);
    }
    
private:
    ngx_command_t command_;
};

// 命令集合类
class NgxCommandSet {
public:
    // 添加命令
    void add_command(const NgxCommand& cmd) {
        commands_.push_back(cmd.get_raw());
    }
    
    // 获取命令数组指针
    ngx_command_t* get_raw() {
        if (commands_.empty()) {
            return nullptr;
        }
        
        // 确保最后一个命令是null命令
        if (commands_.back().name.len != 0 || commands_.back().name.data != nullptr) {
            commands_.push_back(NgxCommand::null_command().get_raw());
        }
        
        return commands_.data();
    }
    
private:
    std::vector<ngx_command_t> commands_;
};
```

## 7. 完整示例

以下是使用上述封装的完整模块示例：

```cpp
class BlogModule {
public:
    // 初始化模块
    static bool initialize() {
        // 创建命令
        NgxCommandSet commands;
        commands.add_command(NgxCommand(
            "blog_path",
            NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
            handle_blog_path,
            NGX_HTTP_LOC_CONF_OFFSET,
            offsetof(BlogConfig, path)
        ));
        
        // 添加其他命令...
        
        // 定义模块上下文
        static ngx_http_module_t module_ctx = {
            preconfiguration,
            postconfiguration,
            create_main_conf,
            init_main_conf,
            create_srv_conf,
            merge_srv_conf,
            create_loc_conf,
            merge_loc_conf
        };
        
        // 定义模块
        blog_module = {
            NGX_MODULE_V1,
            &module_ctx,
            commands.get_raw(),
            NGX_HTTP_MODULE,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            NGX_MODULE_V1_PADDING
        };
        
        return true;
    }
    
    // 处理请求
    static ngx_int_t handle_request(ngx_http_request_t* r) {
        // 使用C++封装
        NgxHttpRequest request(r);
        
        // 获取配置
        auto* conf = request.get_module_loc_conf<BlogConfig>(blog_module);
        
        // 构建响应
        std::string response = "<html><body><h1>Blog</h1><p>Welcome!</p></body></html>";
        
        // 发送响应
        return request.send_response(NGX_HTTP_OK, "text/html", response);
    }
    
    // ... 其他回调函数
    
private:
    static ngx_module_t blog_module;
};
```

通过这些封装，您可以使用现代C++的方式来开发Nginx模块，使代码更加安全、简洁和易于维护。

## 8. Nginx基类设计

为了提高代码复用性并遵循面向对象设计原则，我们可以创建一个基类来管理Nginx数据结构指针，然后让其他类继承该基类。

### 8.1 NgxContext基类

以下是一个通用的NgxContext基类设计：

```cpp
/**
 * @brief Nginx上下文基类，作为所有Nginx数据结构封装的基础
 * @tparam T Nginx C结构类型
 */
template <typename T>
class NgxContext {
public:
    /**
     * @brief 从原始指针构造上下文
     * @param ptr 原始Nginx结构指针
     * @param owns_ptr 是否拥有指针（是否负责释放）
     */
    explicit NgxContext(T* ptr, bool owns_ptr = false)
        : ptr_(ptr), owns_ptr_(owns_ptr) {}
    
    /**
     * @brief 虚析构函数
     */
    virtual ~NgxContext() {
        // 如果拥有指针并且需要释放，则派生类应重写此方法
        // 基类不直接管理释放，因为不同Nginx结构有不同的释放方法
    }
    
    /**
     * @brief 获取原始指针
     * @return 原始Nginx结构指针
     */
    T* get_raw() const { 
        return ptr_; 
    }
    
    /**
     * @brief 检查指针是否有效
     * @return 指针是否有效
     */
    bool is_valid() const { 
        return ptr_ != nullptr; 
    }
    
    /**
     * @brief 获取记录器
     * @return Nginx日志对象
     */
    ngx_log_t* get_log() const {
        // 派生类应重写此方法以返回适当的日志对象
        return nullptr;
    }
    
protected:
    T* ptr_ = nullptr;          // 原始Nginx结构指针
    bool owns_ptr_ = false;     // 是否拥有指针（是否负责释放）
};
```

### 8.2 请求上下文类

重构NgxHttpRequest为继承NgxContext的类：

```cpp
/**
 * @brief HTTP请求上下文类
 */
class NgxHttpRequest : public NgxContext<ngx_http_request_t> {
public:
    /**
     * @brief 从原始请求指针构造
     * @param r 原始请求指针
     */
    explicit NgxHttpRequest(ngx_http_request_t* r)
        : NgxContext<ngx_http_request_t>(r) {}
    
    /**
     * @brief 获取请求方法
     * @return 请求方法名称
     */
    std::string get_method() const {
        if (!is_valid()) {
            return "INVALID";
        }
        
        if (ptr_->method_name.len > 0) {
            return std::string(
                reinterpret_cast<char*>(ptr_->method_name.data),
                ptr_->method_name.len);
        }
        
        // 如果没有方法名，返回方法ID
        switch (ptr_->method) {
            case NGX_HTTP_GET:
                return "GET";
            case NGX_HTTP_POST:
                return "POST";
            case NGX_HTTP_HEAD:
                return "HEAD";
            // ... 其他方法
            default:
                return "UNKNOWN";
        }
    }
    
    /**
     * @brief 获取URI
     * @return URI字符串封装
     */
    NgxString get_uri() const {
        if (!is_valid()) {
            return NgxString(ngx_string(""));
        }
        return NgxString(ptr_->uri);
    }
    
    // ... 其他方法 ...
    
    /**
     * @brief 重写获取日志方法
     * @return 请求的日志对象
     */
    ngx_log_t* get_log() const override {
        if (!is_valid()) {
            return nullptr;
        }
        return ptr_->connection->log;
    }
};
```

### 8.3 配置上下文类

重构NgxConfContext为继承NgxContext的类：

```cpp
/**
 * @brief 配置上下文类
 */
class NgxConfContext : public NgxContext<ngx_conf_t> {
public:
    /**
     * @brief 从原始配置指针构造
     * @param conf 原始配置指针
     */
    explicit NgxConfContext(ngx_conf_t* conf)
        : NgxContext<ngx_conf_t>(conf) {}
    
    /**
     * @brief 获取参数数组
     * @return 参数数组
     */
    std::vector<NgxString> get_args() const {
        std::vector<NgxString> result;
        if (!is_valid() || !ptr_->args) {
            return result;
        }
        
        ngx_str_t* values = static_cast<ngx_str_t*>(ptr_->args->elts);
        
        for (ngx_uint_t i = 0; i < ptr_->args->nelts; ++i) {
            result.emplace_back(values[i]);
        }
        
        return result;
    }
    
    // ... 其他方法 ...
    
    /**
     * @brief 重写获取日志方法
     * @return 配置的日志对象
     */
    ngx_log_t* get_log() const override {
        if (!is_valid()) {
            return nullptr;
        }
        return ptr_->log;
    }
};
```

### 8.4 内存池上下文类

重构NgxPool为继承NgxContext的类：

```cpp
/**
 * @brief 内存池上下文类
 */
class NgxPool : public NgxContext<ngx_pool_t> {
public:
    /**
     * @brief 创建新的内存池
     * @param size 内存池大小
     */
    explicit NgxPool(size_t size = NGX_DEFAULT_POOL_SIZE)
        : NgxContext<ngx_pool_t>(ngx_create_pool(size, nullptr), true) {
        if (!is_valid()) {
            throw std::bad_alloc();
        }
    }
    
    /**
     * @brief 从现有池构造
     * @param pool 现有内存池指针
     * @param owns 是否拥有池（负责释放）
     */
    explicit NgxPool(ngx_pool_t* pool, bool owns = false)
        : NgxContext<ngx_pool_t>(pool, owns) {}
    
    /**
     * @brief 析构函数，释放内存池
     */
    ~NgxPool() override {
        if (is_valid() && owns_ptr_) {
            ngx_destroy_pool(ptr_);
            ptr_ = nullptr;
        }
    }
    
    /**
     * @brief 分配内存
     * @tparam T 需要分配的类型
     * @return 分配的内存指针
     */
    template<typename T>
    T* alloc() {
        if (!is_valid()) {
            throw std::runtime_error("Invalid pool");
        }
        
        void* p = ngx_pcalloc(ptr_, sizeof(T));
        if (p == nullptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(p);
    }
    
    // ... 其他方法 ...
    
    /**
     * @brief 重写获取日志方法
     * @return 内存池的日志对象
     */
    ngx_log_t* get_log() const override {
        if (!is_valid()) {
            return nullptr;
        }
        return ptr_->log;
    }
};
```

### 8.5 扩展设计 - 模块上下文类

为了方便模块开发，我们可以创建一个模块上下文类：

```cpp
/**
 * @brief 模块上下文类
 */
class NgxModuleContext : public NgxContext<ngx_module_t> {
public:
    /**
     * @brief 从原始模块指针构造
     * @param module 原始模块指针
     */
    explicit NgxModuleContext(ngx_module_t* module)
        : NgxContext<ngx_module_t>(module) {}
    
    /**
     * @brief 获取模块类型
     * @return 模块类型字符串
     */
    std::string get_type() const {
        if (!is_valid()) {
            return "INVALID";
        }
        
        switch (ptr_->type) {
            case NGX_CORE_MODULE:
                return "CORE";
            case NGX_EVENT_MODULE:
                return "EVENT";
            case NGX_HTTP_MODULE:
                return "HTTP";
            case NGX_MAIL_MODULE:
                return "MAIL";
            case NGX_STREAM_MODULE:
                return "STREAM";
            default:
                return "UNKNOWN";
        }
    }
    
    /**
     * @brief 获取模块名称
     * @return 模块名称
     */
    std::string get_name() const {
        if (!is_valid() || !ptr_->name) {
            return "unnamed";
        }
        return std::string(reinterpret_cast<const char*>(ptr_->name));
    }
    
    /**
     * @brief 获取模块命令
     * @return 模块命令数组
     */
    std::vector<NgxCommand> get_commands() const {
        std::vector<NgxCommand> result;
        if (!is_valid() || !ptr_->commands) {
            return result;
        }
        
        for (ngx_command_t* cmd = ptr_->commands; cmd->name.len; ++cmd) {
            result.emplace_back(*cmd);
        }
        
        return result;
    }
};
```

### 8.6 使用示例

下面是一个演示如何使用这些基于基类的封装类的示例：

```cpp
// 模块回调函数
ngx_int_t handle_blog_request(ngx_http_request_t* r) {
    // 创建请求上下文
    NgxHttpRequest request(r);
    
    // 获取URI
    NgxString uri = request.get_uri();
    
    // 记录日志
    ngx_log_error(NGX_LOG_INFO, request.get_log(), 0, 
                  "Processing request for URI: %V", &uri.to_ngx_str());
    
    // 创建内存池
    NgxPool pool(r->pool);
    
    // 创建响应
    std::string response = "<html><body><h1>Blog</h1></body></html>";
    
    // 发送响应
    return request.send_response(NGX_HTTP_OK, "text/html", response);
}

// 配置处理函数
char* handle_blog_path(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    // 创建配置上下文
    NgxConfContext ctx(cf);
    
    // 获取参数
    auto args = ctx.get_args();
    if (args.size() < 2) {
        ngx_conf_log_error(NGX_LOG_EMERG, ctx.get_log(), 0,
                         "blog_path requires a parameter");
        return NGX_CONF_ERROR;
    }
    
    // 获取配置
    auto* bmconf = static_cast<BlogModuleConfig*>(conf);
    
    // 设置路径
    ngx_str_t path = args[1].to_ngx_str();
    bmconf->base_path = path;
    
    return NGX_CONF_OK;
}
```

通过这种基类设计，我们可以：

1. 减少重复代码，将共同的功能集中在基类中
2. 提供一致的接口，所有Nginx结构封装类都继承自同一个基类
3. 实现多态行为，可以根据具体需求扩展和特化不同的Nginx结构
4. 简化错误处理，在基类中统一提供验证方法
5. 统一日志处理，通过基类提供一致的日志接口

这种设计特别适合大型Nginx模块项目，可以显著提高代码的可维护性和可扩展性。 