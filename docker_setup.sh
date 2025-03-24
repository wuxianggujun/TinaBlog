#!/bin/bash

# 显示彩色输出的函数
print_info() {
    echo -e "\e[1;34m[INFO] $1\e[0m"
}

print_success() {
    echo -e "\e[1;32m[SUCCESS] $1\e[0m"
}

print_error() {
    echo -e "\e[1;31m[ERROR] $1\e[0m"
}

print_warning() {
    echo -e "\e[1;33m[WARNING] $1\e[0m"
}

echo "======================================================"
echo "           TinaBlog Docker环境设置脚本                "
echo "======================================================"
echo ""

# 获取当前目录路径
PROJECT_DIR=$(pwd)
print_info "项目目录: $PROJECT_DIR"

# 检查Docker是否已安装
if ! command -v docker &> /dev/null; then
    print_error "未安装Docker！请先安装Docker，然后再运行此脚本"
    echo "安装说明: https://docs.docker.com/get-docker/"
    exit 1
fi

print_success "检测到Docker已安装"

# 创建Dockerfile
print_info "创建Dockerfile..."
cat > Dockerfile << EOF
FROM ubuntu:latest

# 安装基本工具和依赖
RUN apt-get update && apt-get install -y \\
    git \\
    cmake \\
    g++ \\
    make \\
    curl \\
    unzip \\
    tar \\
    zip \\
    pkg-config \\
    build-essential \\
    libssl-dev \\
    ninja-build \\
    bison \\
    flex \\
    libreadline-dev \\
    zlib1g-dev \\
    autoconf \\
    automake \\
    libtool \\
    uuid-dev \\
    libselinux1-dev \\
    gettext

# 设置工作目录
WORKDIR /app

# 暴露端口（Drogon默认使用8080端口）
EXPOSE 8080

# 默认命令
CMD ["/bin/bash"]
EOF

print_success "Dockerfile创建完成"

# 创建docker-compose.yml文件
print_info "创建docker-compose.yml文件..."
cat > docker-compose.yml << EOF
version: '3'

services:
  tinablog:
    build: .
    volumes:
      - .:/app
    ports:
      - "8080:8080"
    environment:
      - TZ=Asia/Shanghai
    tty: true
    stdin_open: true
EOF

print_success "docker-compose.yml创建完成"

# 创建辅助脚本
print_info "创建辅助脚本..."

# 创建构建容器脚本
cat > docker_build.sh << EOF
#!/bin/bash
docker-compose build
EOF
chmod +x docker_build.sh

# 创建启动容器脚本
cat > docker_run.sh << EOF
#!/bin/bash
docker-compose up -d
docker-compose exec tinablog /bin/bash
EOF
chmod +x docker_run.sh

# 创建一键式构建和运行脚本
cat > docker_build_run.sh << EOF
#!/bin/bash
# 构建Docker镜像
./docker_build.sh

# 运行容器
./docker_run.sh
EOF
chmod +x docker_build_run.sh

print_success "辅助脚本创建完成"

print_info "===================== 使用说明 ====================="
print_info "1. 构建Docker镜像:        ./docker_build.sh"
print_info "2. 启动容器并进入Shell:   ./docker_run.sh"
print_info "3. 一键构建并运行:        ./docker_build_run.sh"
print_info ""
print_info "进入容器后，您可以运行以下命令:"
print_info "cd /app"
print_info "chmod +x install_dependencies.sh"
print_info "./install_dependencies.sh"
print_info "chmod +x build.sh"
print_info "./build.sh"
print_info "cd build && ./TinaBlog"
print_info "======================================================="

print_success "Docker环境设置完成！" 