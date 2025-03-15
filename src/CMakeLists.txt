# 包含必要的检查模块
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)

# 设置CMAKE_MODULE_PATH以包含我们自定义的CMake模块
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# 引入自定义模块
include(CopyDependencies)

# 收集源文件和头文件
# ==================================================

# 设置常用目录变量
set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core)
set(EVENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/event)
set(HTTP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/http) 
set(MAIL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mail)
set(STREAM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stream)
set(BLOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/blog)

# 设置基础包含目录 - 所有平台通用的目录
set(COMMON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CORE_DIR}
    ${EVENT_DIR}
    ${EVENT_DIR}/modules
    ${EVENT_DIR}/quic
    ${HTTP_DIR}
    ${HTTP_DIR}/modules
    ${HTTP_DIR}/v2
    ${MAIL_DIR}
    ${STREAM_DIR} 
    # 自己的模块
    ${BLOG_DIR}
    ${BLOG_DIR}/db  # 添加数据库相关目录
    
    # 第三方库包含目录
    ${CMAKE_SOURCE_DIR}/third_party/pcre/include
    ${CMAKE_SOURCE_DIR}/third_party/zlib
    ${CMAKE_BINARY_DIR}/third_party/zlib  # zconf.h的生成路径
    ${CMAKE_SOURCE_DIR}/third_party/openssl-3.4/include
    
    # 添加libpqxx的包含目录
    ${CMAKE_SOURCE_DIR}/third_party/libpqxx/include
    ${CMAKE_BINARY_DIR}/third_party/libpqxx/include
)

# 设置平台特定的包含目录和编译定义
if(WIN32)
    set(PLATFORM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/os/win32)
    set(PLATFORM_DEFINES
        WIN32
        _WIN32
        NGX_WIN32=1
        NGX_THREADS=0
    )
    set(PLATFORM_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/os/win32)
else()
    set(PLATFORM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/os/unix)
    set(PLATFORM_DEFINES
        _GNU_SOURCE
        _FILE_OFFSET_BITS=64
        NGX_THREADS=1
    )
    set(PLATFORM_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/os/unix)
endif()

# 收集平台无关的头文件
file(GLOB_RECURSE common_HEADERS 
    "${CORE_DIR}/*.h"
    "${EVENT_DIR}/*.h"
    "${HTTP_DIR}/*.h"
    "${MAIL_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/misc/*.h"
    "${STREAM_DIR}/*.h" 
    "${BLOG_DIR}/*.hpp"
)

# 收集平台特定的头文件
file(GLOB_RECURSE platform_HEADERS "${PLATFORM_SRC_DIR}/*.h")

# 设置平台无关的源文件 - 同之前的配置
# 注意这里可以简化，但为了易读性保持原来的结构
set(common_SOURCES
    # 添加模块定义文件
    ${CMAKE_SOURCE_DIR}/ngx_modules.c

    # core模块 - 平台无关部分
    ${CORE_DIR}/nginx.c 
    ${CORE_DIR}/ngx_log.c 
    ${CORE_DIR}/ngx_palloc.c 
    ${CORE_DIR}/ngx_array.c 
    ${CORE_DIR}/ngx_list.c 
    ${CORE_DIR}/ngx_hash.c 
    ${CORE_DIR}/ngx_buf.c 
    ${CORE_DIR}/ngx_queue.c 
    ${CORE_DIR}/ngx_output_chain.c 
    ${CORE_DIR}/ngx_string.c 
    ${CORE_DIR}/ngx_parse.c 
    ${CORE_DIR}/ngx_parse_time.c 
    ${CORE_DIR}/ngx_inet.c 
    ${CORE_DIR}/ngx_file.c 
    ${CORE_DIR}/ngx_crc32.c 
    ${CORE_DIR}/ngx_murmurhash.c 
    ${CORE_DIR}/ngx_md5.c 
    ${CORE_DIR}/ngx_sha1.c 
    ${CORE_DIR}/ngx_rbtree.c 
    ${CORE_DIR}/ngx_radix_tree.c 
    ${CORE_DIR}/ngx_slab.c 
    ${CORE_DIR}/ngx_times.c 
    ${CORE_DIR}/ngx_shmtx.c 
    ${CORE_DIR}/ngx_connection.c 
    ${CORE_DIR}/ngx_cycle.c 
    ${CORE_DIR}/ngx_spinlock.c 
    ${CORE_DIR}/ngx_rwlock.c 
    ${CORE_DIR}/ngx_cpuinfo.c 
    ${CORE_DIR}/ngx_conf_file.c 
    ${CORE_DIR}/ngx_module.c 
    ${CORE_DIR}/ngx_resolver.c 
    ${CORE_DIR}/ngx_open_file_cache.c 
    ${CORE_DIR}/ngx_crypt.c 
    ${CORE_DIR}/ngx_proxy_protocol.c 
    ${CORE_DIR}/ngx_syslog.c
    
    # event模块 - 平台无关部分
    ${EVENT_DIR}/ngx_event.c 
    ${EVENT_DIR}/ngx_event_timer.c 
    ${EVENT_DIR}/ngx_event_posted.c 
    ${EVENT_DIR}/ngx_event_accept.c 
    ${EVENT_DIR}/ngx_event_udp.c 
    ${EVENT_DIR}/ngx_event_connect.c 
    ${EVENT_DIR}/ngx_event_pipe.c
    
    # SSL相关 - 平台无关
    ${EVENT_DIR}/ngx_event_openssl.c 
    ${EVENT_DIR}/ngx_event_openssl_cache.c
    ${EVENT_DIR}/ngx_event_openssl_stapling.c 
    ${CORE_DIR}/ngx_regex.c 

    # HTTP模块 - 平台无关
    ${HTTP_DIR}/ngx_http.c 
    ${HTTP_DIR}/ngx_http_core_module.c 
    ${HTTP_DIR}/ngx_http_special_response.c 
    ${HTTP_DIR}/ngx_http_request.c 
    ${HTTP_DIR}/ngx_http_parse.c 
    ${HTTP_DIR}/modules/ngx_http_log_module.c 
    ${HTTP_DIR}/ngx_http_request_body.c 
    ${HTTP_DIR}/ngx_http_variables.c 
    ${HTTP_DIR}/ngx_http_script.c 
    ${HTTP_DIR}/ngx_http_upstream.c 
    ${HTTP_DIR}/ngx_http_upstream_round_robin.c 
    ${HTTP_DIR}/ngx_http_file_cache.c 
    ${HTTP_DIR}/ngx_http_huff_decode.c
    ${HTTP_DIR}/ngx_http_huff_encode.c
    ${HTTP_DIR}/ngx_http_write_filter_module.c 
    ${HTTP_DIR}/ngx_http_header_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_chunked_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_range_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_gzip_filter_module.c 
    ${HTTP_DIR}/ngx_http_postpone_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_ssi_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_charset_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_sub_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_addition_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_gunzip_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_userid_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_headers_filter_module.c 
    ${HTTP_DIR}/ngx_http_copy_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_not_modified_filter_module.c 
    ${HTTP_DIR}/modules/ngx_http_slice_filter_module.c 
    ${HTTP_DIR}/v2/ngx_http_v2.c 
    ${HTTP_DIR}/v2/ngx_http_v2_table.c 
    ${HTTP_DIR}/v2/ngx_http_v2_encode.c 
    ${HTTP_DIR}/v2/ngx_http_v2_filter_module.c 
    ${HTTP_DIR}/v2/ngx_http_v2_module.c 
    ${HTTP_DIR}/modules/ngx_http_static_module.c 
    ${HTTP_DIR}/modules/ngx_http_gzip_static_module.c 
    ${HTTP_DIR}/modules/ngx_http_dav_module.c 
    ${HTTP_DIR}/modules/ngx_http_autoindex_module.c 
    ${HTTP_DIR}/modules/ngx_http_index_module.c 
    ${HTTP_DIR}/modules/ngx_http_random_index_module.c 
    ${HTTP_DIR}/modules/ngx_http_mirror_module.c 
    ${HTTP_DIR}/modules/ngx_http_try_files_module.c 
    ${HTTP_DIR}/modules/ngx_http_auth_request_module.c 
    ${HTTP_DIR}/modules/ngx_http_auth_basic_module.c 
    ${HTTP_DIR}/modules/ngx_http_access_module.c 
    ${HTTP_DIR}/modules/ngx_http_limit_conn_module.c 
    ${HTTP_DIR}/modules/ngx_http_limit_req_module.c 
    ${HTTP_DIR}/modules/ngx_http_realip_module.c 
    ${HTTP_DIR}/modules/ngx_http_geo_module.c 
    ${HTTP_DIR}/modules/ngx_http_map_module.c 
    ${HTTP_DIR}/modules/ngx_http_split_clients_module.c 
    ${HTTP_DIR}/modules/ngx_http_referer_module.c 
    ${HTTP_DIR}/modules/ngx_http_rewrite_module.c 
    ${HTTP_DIR}/modules/ngx_http_ssl_module.c 
    ${HTTP_DIR}/modules/ngx_http_proxy_module.c 
    ${HTTP_DIR}/modules/ngx_http_fastcgi_module.c 
    ${HTTP_DIR}/modules/ngx_http_uwsgi_module.c 
    ${HTTP_DIR}/modules/ngx_http_scgi_module.c 
    ${HTTP_DIR}/modules/ngx_http_grpc_module.c 
    ${HTTP_DIR}/modules/ngx_http_memcached_module.c 
    ${HTTP_DIR}/modules/ngx_http_empty_gif_module.c 
    ${HTTP_DIR}/modules/ngx_http_browser_module.c 
    ${HTTP_DIR}/modules/ngx_http_secure_link_module.c 
    ${HTTP_DIR}/modules/ngx_http_flv_module.c 
    ${HTTP_DIR}/modules/ngx_http_mp4_module.c 
    ${HTTP_DIR}/modules/ngx_http_upstream_hash_module.c 
    ${HTTP_DIR}/modules/ngx_http_upstream_ip_hash_module.c 
    ${HTTP_DIR}/modules/ngx_http_upstream_least_conn_module.c 
    ${HTTP_DIR}/modules/ngx_http_upstream_random_module.c 
    ${HTTP_DIR}/modules/ngx_http_upstream_keepalive_module.c 
    ${HTTP_DIR}/modules/ngx_http_upstream_zone_module.c 
    ${HTTP_DIR}/modules/ngx_http_stub_status_module.c 

    # MAIL模块 - 平台无关
    ${MAIL_DIR}/ngx_mail.c 
    ${MAIL_DIR}/ngx_mail_core_module.c 
    ${MAIL_DIR}/ngx_mail_handler.c 
    ${MAIL_DIR}/ngx_mail_parse.c 
    ${MAIL_DIR}/ngx_mail_ssl_module.c 
    ${MAIL_DIR}/ngx_mail_pop3_module.c 
    ${MAIL_DIR}/ngx_mail_pop3_handler.c 
    ${MAIL_DIR}/ngx_mail_imap_module.c 
    ${MAIL_DIR}/ngx_mail_imap_handler.c 
    ${MAIL_DIR}/ngx_mail_smtp_module.c 
    ${MAIL_DIR}/ngx_mail_smtp_handler.c 
    ${MAIL_DIR}/ngx_mail_auth_http_module.c 
    ${MAIL_DIR}/ngx_mail_proxy_module.c 
    ${MAIL_DIR}/ngx_mail_realip_module.c 

    # STREAM模块 - 平台无关
    ${STREAM_DIR}/ngx_stream.c 
    ${STREAM_DIR}/ngx_stream_variables.c 
    ${STREAM_DIR}/ngx_stream_script.c 
    ${STREAM_DIR}/ngx_stream_handler.c 
    ${STREAM_DIR}/ngx_stream_core_module.c 
    ${STREAM_DIR}/ngx_stream_log_module.c 
    ${STREAM_DIR}/ngx_stream_proxy_module.c 
    ${STREAM_DIR}/ngx_stream_upstream.c 
    ${STREAM_DIR}/ngx_stream_upstream_round_robin.c 
    ${STREAM_DIR}/ngx_stream_write_filter_module.c 
    ${STREAM_DIR}/ngx_stream_ssl_module.c 
    ${STREAM_DIR}/ngx_stream_limit_conn_module.c 
    ${STREAM_DIR}/ngx_stream_access_module.c 
    ${STREAM_DIR}/ngx_stream_geo_module.c 
    ${STREAM_DIR}/ngx_stream_map_module.c 
    ${STREAM_DIR}/ngx_stream_split_clients_module.c 
    ${STREAM_DIR}/ngx_stream_return_module.c
    ${STREAM_DIR}/ngx_stream_upstream_hash_module.c 
    ${STREAM_DIR}/ngx_stream_upstream_least_conn_module.c 
    ${STREAM_DIR}/ngx_stream_upstream_random_module.c 
    ${STREAM_DIR}/ngx_stream_upstream_zone_module.c 
    ${STREAM_DIR}/ngx_stream_ssl_preread_module.c
    ${STREAM_DIR}/ngx_stream_set_module.c
    ${STREAM_DIR}/ngx_stream_pass_module.c
        
    # Blog模块
    ${BLOG_DIR}/Nginx.cpp
    ${BLOG_DIR}/HttpConfigHelper.cpp
    ${BLOG_DIR}/BlogConfig.cpp
    ${BLOG_DIR}/BlogModule.cpp
    ${BLOG_DIR}/BlogRouter.cpp
    ${BLOG_DIR}/TemplateCacheManager.cpp
    ${BLOG_DIR}/db/DbManager.cpp
    ${BLOG_DIR}/db/BlogPostDao.cpp
)

# 设置平台特定的源文件
if(WIN32)
    set(platform_SOURCES
        # Windows平台特定源文件
        ${PLATFORM_SRC_DIR}/ngx_errno.c 
        ${PLATFORM_SRC_DIR}/ngx_alloc.c 
        ${PLATFORM_SRC_DIR}/ngx_files.c 
        ${PLATFORM_SRC_DIR}/ngx_shmem.c 
        ${PLATFORM_SRC_DIR}/ngx_time.c 
        ${PLATFORM_SRC_DIR}/ngx_process.c 
        ${PLATFORM_SRC_DIR}/ngx_thread.c 
        ${PLATFORM_SRC_DIR}/ngx_socket.c 
        ${PLATFORM_SRC_DIR}/ngx_wsarecv.c 
        ${PLATFORM_SRC_DIR}/ngx_wsarecv_chain.c 
        ${PLATFORM_SRC_DIR}/ngx_udp_wsarecv.c 
        ${PLATFORM_SRC_DIR}/ngx_wsasend.c 
        ${PLATFORM_SRC_DIR}/ngx_wsasend_chain.c 
        ${PLATFORM_SRC_DIR}/ngx_win32_init.c 
        ${PLATFORM_SRC_DIR}/ngx_user.c 
        ${PLATFORM_SRC_DIR}/ngx_dlopen.c 
        ${PLATFORM_SRC_DIR}/ngx_event_log.c 
        ${PLATFORM_SRC_DIR}/ngx_process_cycle.c
        ${EVENT_DIR}/ngx_event_acceptex.c 
        ${EVENT_DIR}/modules/ngx_iocp_module.c 
        ${EVENT_DIR}/modules/ngx_win32_select_module.c 
        ${EVENT_DIR}/modules/ngx_win32_poll_module.c
    )
else()
    set(platform_SOURCES
        # Linux/Unix平台特定源文件
        ${PLATFORM_SRC_DIR}/ngx_errno.c
        ${PLATFORM_SRC_DIR}/ngx_alloc.c
        ${PLATFORM_SRC_DIR}/ngx_files.c
        # ... 其他Unix特定文件 ...
        ${CORE_DIR}/ngx_thread_pool.c  # 只在Unix平台包含
        
        # Unix特定事件模块
        ${EVENT_DIR}/modules/ngx_epoll_module.c
        ${EVENT_DIR}/modules/ngx_kqueue_module.c
        # ... 其他Unix特定事件模块 ...
    )
endif()

# 合并所有源文件和头文件
set(nginx_SOURCES ${common_SOURCES} ${platform_SOURCES})
set(nginx_HEADERS ${common_HEADERS} ${platform_HEADERS})

# 创建可执行文件
add_executable(nginx ${nginx_SOURCES} ${nginx_HEADERS})

# 设置包含目录
target_include_directories(nginx PRIVATE 
    ${COMMON_INCLUDE_DIRS}
    ${PLATFORM_INCLUDE_DIRS}
    ${MySQL_INCLUDE_DIRS}
    "${MYSQL_SERVER_DIR}/include"
    "${MYSQL_CONNECTOR_DIR}/include"
    ${MYSQL_CONNECTOR_INCLUDE_DIRS}
)

# 设置编译定义
target_compile_definitions(nginx PRIVATE ${PLATFORM_DEFINES})

# 链接库 - 使用现代方式
target_link_libraries(nginx
    PRIVATE
    # 系统库 - Windows平台
    $<$<PLATFORM_ID:Windows>:kernel32.lib>
    $<$<PLATFORM_ID:Windows>:user32.lib>
    $<$<PLATFORM_ID:Windows>:advapi32.lib>
    $<$<PLATFORM_ID:Windows>:ws2_32.lib>
    $<$<PLATFORM_ID:Windows>:gdi32.lib>
    $<$<PLATFORM_ID:Windows>:crypt32.lib>
    
    # PCRE库 - 使用生成器表达式选择Debug或Release库
    "$<$<CONFIG:Debug>:${PCRE_LIBRARIES_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${PCRE_LIBRARIES_RELEASE}>"
    
    # ZLIB库
    ${ZLIB_LIBRARIES}
    
    # OpenSSL库
    ${OPENSSL_LIBRARIES}
    
    # Boost库
    Boost::container
    Boost::exception
    Boost::utility
    Boost::tti

    # MySQL库
    "${MYSQL_SERVER_DIR}/lib/libmysql.lib"
    ${MYSQL_CONNECTOR_LIBRARIES}
)

# 设置编译和链接选项
configure_compiler_options(nginx)

# 设置生成位置
set_target_properties(nginx PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# 自动分组源文件 - 使用通用函数
auto_group_sources(nginx ${CMAKE_CURRENT_SOURCE_DIR})

# 安装目标
install(TARGETS nginx DESTINATION ${NGINX_INSTALL_DIR})

# 创建清理动态库目标
create_clean_dlls_target()

# 复制所有依赖的DLL到目标目录
copy_all_dependencies(nginx)

# 创建用于手动复制DLL的目标
create_copy_dlls_target()

# 拷贝nginx原始html文件（如果有）
if(EXISTS "${CMAKE_SOURCE_DIR}/docs/html")
    add_custom_command(TARGET nginx PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_SOURCE_DIR}/docs/html ${CMAKE_SOURCE_DIR}/html
        COMMENT "Copying original nginx html files..."
    )
endif()