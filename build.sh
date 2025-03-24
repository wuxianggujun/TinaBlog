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

# 设置工作目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
cd "$SCRIPT_DIR"

# 检查必要的系统命令
check_system_tools() {
    local missing_tools=()
    
    # 必要的工具
    local tools=("cmake" "g++" "make" "ninja" "zip" "git" "bison" "flex" "autoconf" "automake" "libtool")
    
    for tool in "${tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -gt 0 ]; then
        print_warning "缺少必要的系统工具: ${missing_tools[*]}"
        print_info "请执行install_dependencies.sh安装依赖"
        return 1
    fi
    
    return 0
}

# 输出欢迎信息
echo "======================================================"
echo "           TinaBlog 编译脚本 - Ubuntu版本         "
echo "======================================================"
echo ""

# 检查系统工具
check_system_tools
if [ $? -ne 0 ]; then
    if [ -f "$SCRIPT_DIR/install_dependencies.sh" ]; then
        print_info "发现安装脚本，是否运行安装依赖？(y/n)"
        read -r answer
        if [[ "$answer" =~ ^[Yy]$ ]]; then
            bash "$SCRIPT_DIR/install_dependencies.sh"
            if [ $? -ne 0 ]; then
                print_error "安装依赖失败，请检查错误并重试"
                exit 1
            fi
            # 重新加载环境变量
            if [ -f ~/.bashrc ]; then
                source ~/.bashrc
            fi
        else
            print_error "缺少必要的系统工具，无法继续"
            exit 1
        fi
    else
        print_error "未找到安装脚本，请安装必要的系统工具后重试"
        exit 1
    fi
fi

# 检查vcpkg是否安装
if [ -z "$VCPKG_ROOT" ]; then
    print_warning "未检测到VCPKG_ROOT环境变量"
    
    # 检查是否有安装脚本
    if [ -f "$SCRIPT_DIR/install_dependencies.sh" ]; then
        print_info "发现安装脚本，是否运行安装依赖？(y/n)"
        read -r answer
        if [[ "$answer" =~ ^[Yy]$ ]]; then
            bash "$SCRIPT_DIR/install_dependencies.sh"
            if [ $? -ne 0 ]; then
                print_error "安装依赖失败，请检查错误并重试"
                exit 1
            fi
            # 重新加载环境变量
            if [ -f ~/.bashrc ]; then
                source ~/.bashrc
            fi
        else
            print_error "需要先安装依赖才能继续"
            exit 1
        fi
    else
        print_error "未找到安装脚本，请先运行安装依赖的步骤"
        exit 1
    fi
fi

# 检查build目录
print_info "清理旧的构建文件..."
if [ -d "build" ]; then
    print_info "删除整个build目录..."
    rm -rf build
    
    # 确保目录已被删除
    if [ -d "build" ]; then
        print_warning "常规删除失败，尝试强制删除..."
        find build -type f -exec rm -f {} \;
        find build -type d -exec rm -rf {} \;
        rm -rf build
    fi
fi

# 确保没有残留的CMakeCache.txt
if [ -f "CMakeCache.txt" ]; then
    rm -f CMakeCache.txt
fi

# 检查是否在Docker环境中运行
if grep -q docker /proc/1/cgroup 2>/dev/null || [ -f /.dockerenv ]; then
    print_info "检测到Docker环境，执行特殊清理..."
    # 在Docker中有时需要额外的清理步骤
    find . -name "CMakeCache.txt" -exec rm -f {} \;
    find . -name "cmake_install.cmake" -exec rm -f {} \;
    find . -name "Makefile" -exec rm -f {} \;
    find . -name "CMakeFiles" -type d -exec rm -rf {} \; 2>/dev/null || true
fi

print_info "创建build目录..."
mkdir -p build

# 编译项目
print_info "开始编译项目..."
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
if [ $? -ne 0 ]; then
    print_error "项目配置失败！"
    exit 1
fi

cmake --build .
if [ $? -ne 0 ]; then
    print_error "项目编译失败！"
    exit 1
fi

# 检查编译是否成功
if [ ! -f "TinaBlog" ]; then
    print_error "未找到可执行文件，编译可能未成功完成"
    exit 1
fi

# 检查前端资源
if [ ! -d "html/blog/dist" ]; then
    print_warning "未找到前端资源，检查是否需要创建..."
    mkdir -p html/blog/dist
    
    # 创建一个简单的index.html用于测试
    echo "<html><head><title>TinaBlog</title></head><body><h1>TinaBlog API服务已启动</h1><p>这是一个临时页面，请配置您的前端资源。</p></body></html>" > html/blog/dist/index.html
    
    print_warning "已创建临时前端资源文件用于测试"
fi

print_success "=====================================\n"
print_success "编译成功！\n"
print_success "可执行文件位于: build/TinaBlog\n"
print_success "运行命令: cd build && ./TinaBlog\n"
print_success "=====================================\n"

exit 0 