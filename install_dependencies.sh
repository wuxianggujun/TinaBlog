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

# 检查命令是否存在
check_command() {
    if ! command -v "$1" &> /dev/null; then
        print_warning "未找到命令: $1"
        return 1
    fi
    return 0
}

# 安装系统依赖
install_system_dependencies() {
    print_info "正在安装系统依赖..."
    
    # 检查并安装必要的系统依赖
    local packages=(
        "git"
        "cmake"
        "g++"
        "make"
        "curl"
        "unzip"
        "tar"
        "zip"
        "pkg-config"
        "build-essential"
        "libssl-dev"
        "ninja-build"
        "bison"
        "flex"
        "libreadline-dev"
        "zlib1g-dev"
        "autoconf"
        "automake"
        "libtool"
        "uuid-dev"
        "libselinux1-dev"
        "gettext"
    )
    
    local to_install=()
    
    for pkg in "${packages[@]}"; do
        if ! dpkg -s "$pkg" &> /dev/null; then
            to_install+=("$pkg")
        fi
    done
    
    if [ ${#to_install[@]} -gt 0 ]; then
        print_info "正在安装以下包: ${to_install[*]}"
        sudo apt-get update
        sudo apt-get install -y "${to_install[@]}"
        if [ $? -ne 0 ]; then
            print_error "安装系统依赖失败"
            return 1
        fi
    else
        print_success "系统依赖已安装"
    fi
    
    return 0
}

# 安装vcpkg
install_vcpkg() {
    print_info "检查vcpkg是否已安装..."
    
    # 设置vcpkg安装路径
    local vcpkg_path="$HOME/vcpkg"
    
    # 检查是否已经安装了vcpkg
    if [ -d "$vcpkg_path" ] && [ -f "$vcpkg_path/vcpkg" ]; then
        print_success "vcpkg已安装在 $vcpkg_path"
        
        # 更新vcpkg
        print_info "正在更新vcpkg..."
        cd "$vcpkg_path"
        git pull
        
        # 运行bootstrap
        print_info "正在运行bootstrap..."
        ./bootstrap-vcpkg.sh -disableMetrics
        
        if [ $? -ne 0 ]; then
            print_error "更新vcpkg失败"
            return 1
        fi
    else
        print_info "正在安装vcpkg到 $vcpkg_path..."
        
        # 克隆vcpkg仓库（使用国内镜像加速）
        if ! git clone https://gh-proxy.com/github.com/microsoft/vcpkg.git "$vcpkg_path"; then
            print_warning "从GitHub克隆失败，尝试使用镜像源..."
            if ! git clone https://gitee.com/mirrors/vcpkg.git "$vcpkg_path"; then
                print_error "克隆vcpkg失败，请检查网络连接"
                return 1
            fi
        fi
        
        # 运行bootstrap
        cd "$vcpkg_path"
        ./bootstrap-vcpkg.sh -disableMetrics
        
        if [ $? -ne 0 ]; then
            print_error "安装vcpkg失败"
            return 1
        fi
    fi
    
    # 设置VCPKG_ROOT环境变量
    if ! grep -q "export VCPKG_ROOT=$vcpkg_path" ~/.bashrc; then
        print_info "添加VCPKG_ROOT环境变量到~/.bashrc"
        echo "export VCPKG_ROOT=$vcpkg_path" >> ~/.bashrc
        echo "export PATH=\$PATH:\$VCPKG_ROOT" >> ~/.bashrc
    fi
    
    # 立即设置环境变量以便于当前会话使用
    export VCPKG_ROOT="$vcpkg_path"
    export PATH="$PATH:$VCPKG_ROOT"
    
    print_success "vcpkg安装/更新成功"
    return 0
}

# 安装项目所需的库
install_project_libraries() {
    print_info "正在安装项目依赖库..."
    
    cd "$VCPKG_ROOT"
    
    # 设置平台参数
    local platform="x64-linux"
    
    # 安装drogon
    print_info "正在安装drogon..."
    ./vcpkg install drogon[core,postgres]:${platform} --recurse
    if [ $? -ne 0 ]; then
        print_error "安装drogon失败"
        return 1
    fi
    
    # 安装libpqxx（PostgreSQL客户端库）
    print_info "正在安装libpqxx..."
    ./vcpkg install libpqxx:${platform}
    if [ $? -ne 0 ]; then
        print_error "安装libpqxx失败"
        return 1
    fi
    
    # 安装jwt-cpp
    print_info "正在安装jwt-cpp..."
    ./vcpkg install jwt-cpp:${platform}
    if [ $? -ne 0 ]; then
        print_error "安装jwt-cpp失败"
        return 1
    fi   
    
    # 安装sodium
    print_info "正在安装sodium..."
    ./vcpkg install libsodium:${platform}
    if [ $? -ne 0 ]; then
        print_error "安装sodium失败"
        return 1
    fi    
    
    # 安装sodium
    print_info "正在安装curl..."
    ./vcpkg install curl:${platform}
    if [ $? -ne 0 ]; then
        print_error "安装curl失败"
        return 1
    fi
    
    print_success "所有项目依赖库安装成功"
    return 0
}

# 全局变量定义
# 设置工作目录 - 脚本所在的目录即为项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# 配置项目
configure_project() {
    print_info "配置项目..."
    
    # 回到项目根目录
    cd "$PROJECT_ROOT"
    
    # 检查CMakeLists.txt是否存在
    if [ ! -f "CMakeLists.txt" ]; then
        print_error "未找到CMakeLists.txt文件，无法配置项目"
        return 1
    fi
    
    # 创建build目录
    if [ ! -d "build" ]; then
        mkdir -p build
    fi
    
    # 配置项目
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Debug
    
    if [ $? -ne 0 ]; then
        print_error "配置项目失败"
        return 1
    fi
    
    print_success "项目配置成功"
    return 0
}

# 主函数
main() {
    echo "======================================================"
    echo "           TinaBlog 依赖安装脚本 - Ubuntu版本         "
    echo "======================================================"
    echo ""
    
    # 检查必要的系统命令是否存在
    if ! check_command "git"; then
        print_info "正在安装git..."
        sudo apt-get update && sudo apt-get install -y git
    fi
    
    if ! check_command "cmake"; then
        print_info "正在安装cmake..."
        sudo apt-get update && sudo apt-get install -y cmake
    fi
    
    if ! check_command "zip"; then
        print_info "正在安装zip..."
        sudo apt-get update && sudo apt-get install -y zip
    fi
    
    if ! check_command "bison"; then
        print_info "正在安装bison..."
        sudo apt-get update && sudo apt-get install -y bison
    fi
    
    if ! check_command "flex"; then
        print_info "正在安装flex..."
        sudo apt-get update && sudo apt-get install -y flex
    fi
    
    if ! check_command "autoconf"; then
        print_info "正在安装autoconf..."
        sudo apt-get update && sudo apt-get install -y autoconf automake libtool
    fi

    # 安装系统依赖
    install_system_dependencies
    if [ $? -ne 0 ]; then
        print_error "安装系统依赖失败，退出"
        exit 1
    fi
    
    # 安装vcpkg
    install_vcpkg
    if [ $? -ne 0 ]; then
        print_error "安装vcpkg失败，退出"
        exit 1
    fi
    
    # 安装项目所需的库
    install_project_libraries
    if [ $? -ne 0 ]; then
        print_error "安装项目库失败，退出"
        exit 1
    fi
    
    # 配置项目
    configure_project
    if [ $? -ne 0 ]; then
        print_error "配置项目失败，退出"
        exit 1
    fi
    
    print_success "=====================================\n"
    print_success "依赖安装完成！\n"
    print_success "VCPKG_ROOT环境变量已设置: $VCPKG_ROOT\n"
    print_success "项目配置已完成，可以开始编译和运行\n"
    print_success "编译命令: cd build && cmake --build .\n"
    print_success "运行命令: cd build && ./TinaBlog\n"
    print_success "=====================================\n"
    
    print_info "提示：请关闭并重新打开终端，或运行 'source ~/.bashrc' 使环境变量生效"
    
    return 0
}

# 执行主函数
main
exit $? 