FROM ubuntu:latest

# 避免交互式提示
ENV DEBIAN_FRONTEND=noninteractive

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
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 设置SSH
RUN mkdir /var/run/sshd
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
tail -f /dev/null' > /start.sh && \
    chmod +x /start.sh

# 默认命令
CMD ["/start.sh"]