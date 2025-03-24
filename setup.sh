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
if [ "$(docker ps -q -f name=tinablog)" ]; then
    print_success "TinaBlog容器已启动"
    
    # 显示SSH连接信息
    CONTAINER_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' tinablog)
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
    print_info "主机: postgres (容器内部) 或 localhost (外部)"
    print_info "端口: 5432"
    print_info "数据库: tinablog"
    print_info "用户名: postgres"
    print_info "密码: postgres"
    print_info "=================================="
    
    # 提供进入容器的命令
    print_info "要进入TinaBlog容器，运行:"
    print_info "docker exec -it tinablog bash"
    
    # 提供编译项目的命令
    print_info "要编译项目，进入容器后运行:"
    print_info "cd /app && chmod +x build.sh && ./build.sh"
else
    print_error "容器启动失败，请检查日志:"
    print_error "docker-compose logs"
fi