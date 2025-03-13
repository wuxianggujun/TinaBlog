# TinaBlog - Nginx博客模块

TinaBlog是一个轻量级的基于Nginx的博客模块，可以直接集成到Nginx服务器中，提供简单高效的博客功能。

## 功能特点

- 直接集成到Nginx，无需额外的应用服务器
- 高性能、低资源占用
- 支持Markdown格式的博客文章
- 简单的分类和标签系统
- 文章评论功能
- 缓存系统提高性能
- 跨平台支持（Windows、Linux）

## 系统要求

- Nginx 1.18+
- C++14 兼容的编译器
- CMake 3.10+
- SQLite 3

## 编译安装

### 在Windows上编译

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 在Linux上编译

```bash
mkdir build
cd build
cmake ..
make
```

## 配置使用

1. 将编译好的模块复制到Nginx的模块目录：
   - Windows: `copy build\blog\ngx_http_blog_module.dll C:\nginx\modules\`
   - Linux: `cp build/blog/ngx_http_blog_module.so /usr/local/nginx/modules/`

2. 在Nginx配置文件中添加模块配置：

```nginx
# 全局配置
load_module modules/ngx_http_blog_module.so;  # 或 .dll (Windows)

# http 块内配置
blog_root html/blog;
blog_db_path html/blog/data/blog.db;
blog_cache_time 3600;
blog_enable_comment on;

# server 块内配置
server {
    # ...

    location /blog {
        blog_enable on;
        blog_template_path html/blog/templates;
    }
}
```

3. 创建必要的目录结构：

```
html/
  ├── blog/
  │    ├── templates/  # 模板文件
  │    ├── data/       # 数据存储
  │    └── static/     # 静态资源
```

4. 使用提供的测试脚本生成测试数据：

```bash
# Windows
scripts\generate_test_data.bat

# Linux
./scripts/generate_test_data.sh
```

## 测试

项目包含测试脚本，可以快速测试模块功能：

```bash
# Windows
scripts\test_module.bat

# Linux
./scripts/test_module.sh
```

测试默认在 http://localhost:8088/blog 上运行。

## 目录结构

```
TinaBlog/
  ├── src/             # 源代码
  │    ├── blog/       # 博客模块主要代码
  │    └── utils/      # 工具函数和辅助类
  ├── include/         # 头文件
  ├── scripts/         # 脚本工具
  ├── tests/           # 测试文件
  │    └── templates/  # 测试用模板
  ├── conf/            # 配置文件
  └── build/           # 构建目录（自动生成）
```

## 开发计划

- [ ] 支持用户认证系统
- [ ] 添加更多模板主题
- [ ] 支持更多数据库后端
- [ ] 实现插件系统
- [ ] 添加RSS/Atom订阅支持

## 许可证

本项目采用MIT许可证，详见LICENSE文件。