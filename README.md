# TinaBlog

一个基于Drogon框架的博客API后端，使用C++开发。

## 功能特点

- 基于Drogon高性能C++网络框架
- JWT认证系统
- PostgreSQL数据库支持
- RESTful API设计
- 跨平台支持（Windows、Linux）

## 环境要求

- C++17兼容的编译器（GCC 7+、Clang 5+、MSVC 2019+）
- CMake 3.16+
- vcpkg包管理器
- PostgreSQL数据库服务器

## 依赖库

- Drogon - C++网络框架
- libpqxx - PostgreSQL C++客户端库
- jwt-cpp - JWT实现库

## 快速开始

### Linux环境

1. **安装依赖**

   执行安装脚本：
   ```bash
   chmod +x install_dependencies.sh
   ./install_dependencies.sh
   ```
   
   该脚本会：
   - 安装系统依赖
   - 安装并配置vcpkg
   - 安装所需的C++库
   - 设置环境变量

2. **编译项目**

   使用编译脚本：
   ```bash
   chmod +x build.sh
   ./build.sh
   ```

3. **运行应用**

   ```bash
   cd build
   ./TinaBlog
   ```

### Windows环境

1. **安装依赖**

   - 安装vcpkg：
     ```powershell
     git clone https://github.com/microsoft/vcpkg.git
     cd vcpkg
     .\bootstrap-vcpkg.bat
     ```

   - 设置环境变量`VCPKG_ROOT`指向vcpkg安装路径

   - 安装依赖库：
     ```powershell
     vcpkg install drogon:x64-windows
     vcpkg install libpqxx:x64-windows
     vcpkg install jwt-cpp:x64-windows
     ```

2. **使用CMake构建**

   ```powershell
   mkdir build
   cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   cmake --build .
   ```

### Docker环境

1. **使用docker_setup.sh脚本设置Docker环境**

   ```bash
   # 给脚本添加执行权限
   chmod +x docker_setup.sh
   
   # 运行设置脚本
   ./docker_setup.sh
   ```
   
   该脚本会自动：
   - 创建Dockerfile和docker-compose.yml
   - 创建便捷的Docker操作脚本
   - 提供详细的使用说明

2. **使用生成的Docker脚本**

   ```bash
   # 构建Docker镜像
   ./docker_build.sh
   
   # 启动容器并进入Shell
   ./docker_run.sh
   
   # 或者一键构建并运行
   ./docker_build_run.sh
   ```

3. **在Docker容器内编译运行**

   ```bash
   # 在容器内执行
   cd /app
   chmod +x install_dependencies.sh
   ./install_dependencies.sh
   chmod +x build.sh
   ./build.sh
   cd build && ./TinaBlog
   ```

4. **手动Docker设置（不使用脚本）**

   ```bash
   # 拉取基础镜像
   docker pull ubuntu:latest
   
   # 创建并进入容器，挂载项目目录
   docker run -it -v /本地项目路径:/app ubuntu:latest
   
   # 在容器内安装基本工具
   apt-get update
   apt-get install -y git cmake g++ make curl unzip
   
   # 进入项目目录并执行脚本
   cd /app
   chmod +x install_dependencies.sh
   ./install_dependencies.sh
   chmod +x build.sh
   ./build.sh
   ```

## 注意事项

- **在不同系统间切换时**：使用`build.sh`脚本会自动清理build目录，避免Windows和Linux路径混合导致的编译问题。

- **环境变量**：安装脚本会自动设置环境变量，但可能需要重启终端才能生效。或者手动执行：
  ```bash
  source ~/.bashrc
  ```

- **Docker环境**：
  1. 使用`docker_setup.sh`脚本可以快速配置Docker环境
  2. 在容器内运行时，`build.sh`脚本会自动检测Docker环境并执行特殊清理步骤
  3. 如果遇到路径问题，请参考"故障排除"部分的Docker特定问题解决方案

## 开发指南

### 项目结构

```
TinaBlog/
├── src/
│   └── blog/          # 源代码目录
│       ├── auth/      # 认证相关代码
│       ├── controllers/ # API控制器
│       └── ...
├── html/              # 前端静态资源
├── build.sh           # 编译脚本
├── install_dependencies.sh # 依赖安装脚本
└── CMakeLists.txt     # CMake配置文件
```

### 添加新API

1. 在`src/blog/controllers`目录创建新控制器文件
2. 按照Drogon框架格式定义控制器类
3. 实现必要的API方法
4. 编译并测试新API

## 故障排除

### 编译错误

- **CMake路径错误**：如果遇到路径不匹配错误，运行`build.sh`脚本将会清理build目录并重新编译。

- **依赖库缺失**：运行`install_dependencies.sh`确保安装了所有必要库。

- **环境变量问题**：确保设置了正确的`VCPKG_ROOT`环境变量。

- **Docker特定问题**：
  - 如果出现CMakeCache.txt路径不匹配问题，可能是由于不同系统间的路径混合导致
  - 尝试手动删除所有CMakeCache.txt文件：`find / -name "CMakeCache.txt" -exec rm -f {} \; 2>/dev/null`
  - 确保Docker容器内有足够的权限操作挂载的目录
  - 如果修改build.sh后仍有问题，可以在容器中执行以下命令完全清理构建文件：
    ```bash
    rm -rf build/
    find . -name "CMakeCache.txt" -exec rm -f {} \;
    find . -name "cmake_install.cmake" -exec rm -f {} \;
    find . -name "Makefile" -exec rm -f {} \;
    find . -name "CMakeFiles" -type d -exec rm -rf {} \; 2>/dev/null || true
    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Debug
    ```
  - 在Docker中构建时如遇到权限问题，可以将挂载目录的所有权更改为当前用户：
    ```bash
    chown -R $(id -u):$(id -g) /app
    ```

## 许可证

[在此添加项目许可证信息]

## 数据库配置

项目使用PostgreSQL数据库。有几种配置方式：

### Docker环境

在Docker环境中，docker-compose.yml已配置好PostgreSQL服务和所需的环境变量：

```yaml
services:
  tinablog:
    environment:
      - DB_HOST=postgres  # PostgreSQL服务的主机名
      - DB_PORT=5432      # PostgreSQL服务的端口
      - DB_NAME=tinablog  # 要使用的数据库名
      - DB_USER=postgres  # 数据库用户名
      - DB_PASSWORD=postgres  # 数据库密码
    depends_on:
      - postgres  # 依赖于postgres服务

  postgres:
    image: postgres:latest
    environment:
      - POSTGRES_PASSWORD=postgres
      - POSTGRES_USER=postgres
      - POSTGRES_DB=tinablog
```

无需额外配置，运行`docker-compose up`后应用将自动连接到PostgreSQL服务。

### 本地环境

如果在本地运行应用而不是Docker容器中，您需要：

1. 安装PostgreSQL数据库服务器
2. 创建数据库和用户
3. 设置环境变量或修改代码中的默认连接值

可以设置以下环境变量来配置数据库连接：
- `DB_HOST`：数据库主机地址（默认：localhost）
- `DB_PORT`：数据库端口（默认：5432）
- `DB_NAME`：数据库名称（默认：blog）
- `DB_USER`：数据库用户名（默认：postgres）
- `DB_PASSWORD`：数据库密码（默认需要修改）

#### 在Linux/macOS上设置环境变量：

```bash
export DB_HOST=localhost
export DB_PORT=5432
export DB_NAME=tinablog
export DB_USER=postgres
export DB_PASSWORD=your_password
```

#### 在Windows上设置环境变量：

```powershell
$env:DB_HOST = "localhost"
$env:DB_PORT = "5432"
$env:DB_NAME = "tinablog"
$env:DB_USER = "postgres"
$env:DB_PASSWORD = "your_password"
```