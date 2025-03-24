# TinaBlog Docker部署指南

## 目录

1. [项目概述](#项目概述)
2. [前期准备](#前期准备)
3. [Dockerfile配置](#Dockerfile配置)
4. [Docker Compose配置](#docker-compose配置)
5. [Windows环境部署指南](#windows环境部署指南)
6. [Linux环境部署指南](#linux环境部署指南)
7. [访问与使用指南](#访问与使用指南)
8. [故障排除](#故障排除)

## 项目概述

TinaBlog是一个基于Drogon框架的C++博客系统，使用PostgreSQL作为数据库，支持JWT认证。本指南提供在Docker容器中部署TinaBlog的完整步骤。

## 前期准备

### Windows环境
- 安装 [Docker Desktop for Windows](https://www.docker.com/products/docker-desktop/)
- 确保WSL2已启用
- 安装Git（可选，用于克隆代码）

### Linux环境
- 安装Docker和Docker Compose:
```bash
# 安装Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# 安装Docker Compose
sudo apt-get install docker-compose-plugin
```

## Dockerfile配置

Dockerfile已经配置了必要的开发环境，包括：

- 基本构建工具：gcc/g++, make, cmake
- ninja构建系统：用于更快的编译
- 调试工具：gdb用于C++程序调试
- 数据库客户端
- SSH服务器用于远程访问

完整的Dockerfile如下：

```dockerfile
FROM ubuntu:latest

# 避免交互式提示
ENV DEBIAN_FRONTEND=noninteractive

# 添加中国镜像源
RUN sed -i 's/archive.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list

# 安装curl（用于一键换源脚本）
RUN apt-get update && apt-get install -y curl

# 一键换源脚本（使用非交互式参数）
RUN curl -sSL https://linuxmirrors.cn/main.sh | bash -s -- \
  --source mirrors.ustc.edu.cn \
  --protocol https \
  --use-intranet-source false \
  --backup true \
  --upgrade-software false \
  --clean-cache false

# 安装必要的包
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    g++ \
    make \
    curl \
    unzip \
    tar \
    zip \
    pkg-config \
    build-essential \
    libssl-dev \
    ninja-build \
    bison \
    flex \
    libreadline-dev \
    zlib1g-dev \
    autoconf \
    automake \
    libtool \
    uuid-dev \
    openssh-server \
    sudo \
    vim \
    gdb \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 设置SSH
RUN mkdir -p /var/run/sshd
RUN echo 'root:tinablog' | chpasswd
RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config

# SSH登录修复
RUN sed -i 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' /etc/pam.d/sshd

# 创建非root用户
RUN useradd -m -s /bin/bash developer && \
    echo "developer:developer" | chpasswd && \
    adduser developer sudo && \
    echo "developer ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/developer

# 设置工作目录
WORKDIR /app

# 暴露SSH和应用端口
EXPOSE 22 8080

# 创建启动脚本
RUN echo '#!/bin/bash\n\
service ssh start\n\
echo "SSH服务已启动，监听22端口"\n\
echo "使用以下凭据通过SSH连接:"\n\
echo "  用户: developer"\n\
echo "  密码: developer"\n\
echo "持续运行中..."\n\
tail -f /dev/null' > /start.sh && \
    chmod +x /start.sh

# 默认命令
CMD ["/start.sh"]
```

## Docker Compose配置

在项目根目录创建`docker-compose.yml`文件：

```yaml
version: '3'

services:
  tinablog-linux-dev:
    build:
      context: .
      dockerfile: Dockerfile
    container_name: tinablog-linux-dev
    volumes:
      - .:/app
    ports:
      - "8080:8080"  # 应用端口
      - "2222:22"    # SSH端口
    environment:
      - TZ=Asia/Shanghai
      - DB_HOST=postgres-db
      - DB_PORT=5432
      - DB_NAME=tinablog
      - DB_USER=postgres
      - DB_PASSWORD=postgres
    networks:
      - tinablog-network
    depends_on:
      - postgres-db
    restart: unless-stopped

  postgres-db:
    image: postgres:latest
    container_name: postgres-db
    environment:
      - POSTGRES_PASSWORD=postgres
      - POSTGRES_USER=postgres
      - POSTGRES_DB=tinablog
    volumes:
      - postgres-data:/var/lib/postgresql/data
    ports:
      - "5432:5432"
    networks:
      - tinablog-network
    restart: unless-stopped

volumes:
  postgres-data:

networks:
  tinablog-network:
    driver: bridge
```

## Windows环境部署指南

### 步骤1: 准备项目文件

1. 确保已创建上述`Dockerfile`和`docker-compose.yml`文件
2. 打开PowerShell或命令提示符

### 步骤2: 启动Docker环境

```powershell
# 导航到项目目录
cd C:\Users\wuxianggujun\CodeSpace\CMakeProjects\TinaBlog

# 构建和启动容器
docker-compose up -d

# 检查容器状态
docker ps
```

### 步骤3: 通过SSH连接容器

#### 方法1: 使用OpenSSH客户端

```powershell
# 如果Windows已安装OpenSSH客户端
ssh developer@localhost -p 2222
# 密码: developer
```

#### 方法2: 使用PuTTY

1. 下载并安装[PuTTY](https://www.putty.org/)
2. 配置连接:
   - 主机: `localhost`
   - 端口: `2222`
   - 连接类型: SSH
3. 点击"打开"按钮
4. 登录凭据:
   - 用户名: `developer`
   - 密码: `developer`

#### 方法3: 直接进入容器

```powershell
docker exec -it tinablog-linux-dev bash
```

### 步骤4: 编译和运行TinaBlog

```bash
# 在容器内执行
cd /app
chmod +x build.sh
./build.sh
cd build
./TinaBlog
```

### 步骤5: 访问应用

在浏览器中访问: `http://localhost:8080`

## Linux环境部署指南

### 步骤1: 准备项目文件

```bash
# 克隆项目(如适用)
git clone <项目仓库URL>
cd TinaBlog

# 确保已创建Dockerfile和docker-compose.yml文件
```

### 步骤2: 创建部署脚本

创建`deploy.sh`脚本：

```bash
#!/bin/bash

# 彩色输出
print_info() {
    echo -e "\e[1;34m[INFO] $1\e[0m"
}

print_success() {
    echo -e "\e[1;32m[SUCCESS] $1\e[0m"
}

print_error() {
    echo -e "\e[1;31m[ERROR] $1\e[0m"
}

print_info "启动TinaBlog Docker环境..."

# 检查Docker是否已安装
if ! command -v docker &> /dev/null || ! command -v docker-compose &> /dev/null; then
    print_error "需要Docker和Docker Compose才能继续！"
    exit 1
fi

# 构建和启动容器
print_info "构建和启动容器..."
docker-compose up -d

# 等待容器启动
print_info "等待容器启动..."
sleep 5

# 检查容器状态
if [ "$(docker ps -q -f name=tinablog-linux-dev)" ]; then
    print_success "开发容器已启动"
    
    # 显示SSH连接信息
    print_info "========= SSH连接信息 ============="
    print_info "主机: localhost"
    print_info "端口: 2222"
    print_info "用户名: developer"
    print_info "密码: developer"
    print_info "----------------------"
    print_info "SSH命令: ssh developer@localhost -p 2222"
    print_info "=================================="
    
    # 显示PostgreSQL连接信息
    print_info "========= 数据库连接信息 ============="
    print_info "主机: postgres-db (容器内部) 或 localhost (外部)"
    print_info "端口: 5432"
    print_info "数据库: tinablog"
    print_info "用户名: postgres"
    print_info "密码: postgres"
    print_info "=================================="
    
    # 提供进入容器的命令
    print_info "要进入开发容器，运行:"
    print_info "docker exec -it tinablog-linux-dev bash"
    
    # 提供编译项目的命令
    print_info "要编译项目，进入容器后运行:"
    print_info "cd /app && chmod +x build.sh && ./build.sh"
else
    print_error "容器启动失败，请检查日志:"
    print_error "docker-compose logs"
fi
```

### 步骤3: 执行部署

```bash
# 添加执行权限
chmod +x deploy.sh

# 运行部署脚本
./deploy.sh
```

### 步骤4: 通过SSH连接容器

```bash
ssh developer@localhost -p 2222
# 密码: developer
```

### 步骤5: 编译和运行TinaBlog

```bash
# 在容器内执行
cd /app
chmod +x build.sh
./build.sh
cd build
./TinaBlog
```

### 步骤6: 访问应用

在浏览器中访问: `http://localhost:8080`

## 访问与使用指南

### 1. 应用访问

- Web界面: `http://localhost:8080`
- API端点: `http://localhost:8080/api/`

### 2. 数据库访问

#### 从容器内部访问数据库

```bash
# 安装PostgreSQL客户端
apt-get update && apt-get install -y postgresql-client

# 连接到数据库
psql -h postgres-db -U postgres -d tinablog
# 密码: postgres
```

#### 从主机访问数据库

- 主机: `localhost`
- 端口: `5432`
- 数据库: `tinablog`
- 用户名: `postgres`
- 密码: `postgres`

可以使用pgAdmin、DBeaver或其他PostgreSQL客户端工具连接。

### 3. 更新代码并重新编译

```bash
# 在容器内执行
cd /app
git pull  # 如果使用git管理代码
chmod +x build.sh
./build.sh
cd build
./TinaBlog
```

## 故障排除

### 1. 容器启动问题

```bash
# 查看容器日志
docker logs tinablog-linux-dev
docker logs postgres-db

# 检查容器状态
docker ps -a
```

### 2. 网络连接问题

```bash
# 进入开发容器
docker exec -it tinablog-linux-dev bash

# 测试与数据库的连接
ping postgres-db
nc -zv postgres-db 5432

# 查看网络配置
docker network inspect tinablog-network
```

### 3. 数据库连接错误

如果应用报告无法连接到数据库:

```bash
# 检查环境变量
docker exec -it tinablog-linux-dev env | grep DB_

# 验证PostgreSQL容器正在运行
docker ps | grep postgres-db

# 检查数据库是否已创建
docker exec -it postgres-db psql -U postgres -c "\l"
```

### 4. 权限问题

```bash
# 如果遇到文件权限问题
docker exec -it tinablog-linux-dev bash
sudo chown -R developer:developer /app
```

### 5. 清理环境

```bash
# 停止并移除容器
docker-compose down

# 完全清理（包括卷）
docker-compose down -v
```

希望这个部署指南对您有所帮助！如有任何问题，请查看故障排除部分或提出具体问题。