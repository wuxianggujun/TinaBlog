# Tina博客系统

使用C++20和Drogon框架开发的博客系统。

## 技术栈

- **后端**：C++20, Drogon Framework 1.8.x, PostgreSQL 17.x
- **前端**：Vue.js 3, TailwindCSS, TypeScript

## 特点

- 命名空间管理的API版本控制
- 基于JWT的用户认证
- 文章的CRUD操作
- 分类和标签管理
- 响应式前端设计

## 目录结构

```
src/blog/               # 后端工程
├─controllers/          # 控制器
│  ├─PostController.*   # 文章管理
│  ├─AuthController.*   # 用户认证
│  ├─HomeController.*   # 首页功能
│  └─HealthController.* # 健康检查
├─auth/                 # Jwt验证
├─db/                   # 数据库和SQL语句
│  ├─DbManager.*        # 数据库管理
│  └─schema.sql         # 数据库结构
└─utils/                # 工具类
   ├─HttpUtils.hpp      # 响应包装器
   ├─ErrorCode.hpp      # 响应错误码
   ├─PasswordUtils.hpp  # 密码工具
   └─PasswordUtils.cpp  # 密码工具
```

## API示例

### 文章相关

- `GET /api/articles` - 获取文章列表
- `GET /api/articles/{slug}` - 获取文章详情
- `POST /api/articles` - 创建文章
- `PUT /api/articles/{id}` - 更新文章
- `DELETE /api/articles/{id}` - 删除文章

### 首页相关

- `GET /api/home/featured` - 获取精选文章
- `GET /api/home/recent` - 获取最新文章
- `GET /api/category/{slug}` - 获取分类文章
- `GET /api/tag/{slug}` - 获取标签文章
- `GET /api/stats` - 获取网站统计信息

### 用户相关

- `POST /api/auth/login` - 用户登录
- `POST /api/auth/register` - 用户注册
- `GET /api/auth/profile` - 获取用户资料

## 编译与部署

### 依赖安装

使用vcpkg安装依赖:

```bash
vcpkg install drogon jwt-cpp libsodium nlohmann-json
```

### 编译

```bash
mkdir build && cd build
cmake ..
make
```

### 运行

```bash
./TinaBlog
```

## 贡献与开发

开发规范和详细信息请参考 `.cursorrules` 文件。

## 许可证

MIT