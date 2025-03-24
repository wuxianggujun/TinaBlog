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

# 输出欢迎信息
echo "======================================================"
echo "           TinaBlog 一键运行脚本 - Ubuntu版本         "
echo "======================================================"
echo ""

# 检查编译项目
print_info "检查项目是否需要编译..."

# 首先运行build.sh脚本编译项目
if [ -f "$SCRIPT_DIR/build.sh" ]; then
    print_info "运行编译脚本..."
    bash "$SCRIPT_DIR/build.sh"
    if [ $? -ne 0 ]; then
        print_error "编译失败！请检查错误信息"
        exit 1
    fi
else
    print_error "未找到build.sh脚本，无法继续"
    exit 1
fi

# 进入build目录运行程序
cd "$SCRIPT_DIR/build"
if [ ! -f "./TinaBlog" ]; then
    print_error "编译可能未成功，未找到可执行文件"
    exit 1
fi

print_success "启动TinaBlog服务..."
print_info "按Ctrl+C可停止服务"
echo ""

# 运行程序
./TinaBlog

# 检查程序退出状态
STATUS=$?
if [ $STATUS -ne 0 ]; then
    print_error "程序异常终止，退出码: $STATUS"
    exit $STATUS
fi

exit 0 