# 包含必要的检查模块
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)

# 设置基础包含目录 - 平台无关部分
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/event
    ${CMAKE_CURRENT_SOURCE_DIR}/event/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/event/quic
    ${CMAKE_CURRENT_SOURCE_DIR}/http
    ${CMAKE_CURRENT_SOURCE_DIR}/http/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/http/v2
    ${CMAKE_CURRENT_SOURCE_DIR}/mail
    ${CMAKE_CURRENT_SOURCE_DIR}/stream 
    # 自己的模块
    ${CMAKE_CURRENT_SOURCE_DIR}/blog
    
    # 第三方库包含目录 - 确保包含正确的路径
    ${CMAKE_SOURCE_DIR}/third_party/pcre/include
    ${CMAKE_SOURCE_DIR}/third_party/zlib
    ${CMAKE_BINARY_DIR}/third_party/zlib    # zconf.h的生成路径
    ${CMAKE_SOURCE_DIR}/third_party/openssl-3.4/include
)

# 平台特定包含目录
if(WIN32)
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/os/win32
    )
    
    # Windows平台编译定义
    add_definitions(
        -DWIN32
        -D_WIN32
        -DNGX_WIN32=1
        -DNGX_THREADS=0
    )
else()
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/os/unix
    )
    
    # Linux/Unix平台编译定义
    add_definitions(
        -D_GNU_SOURCE
        -D_FILE_OFFSET_BITS=64
        -DNGX_THREADS=1
    )
endif()

# 收集平台无关的头文件
file(GLOB_RECURSE common_HEADERS 
    "core/*.h"
    "event/*.h"
    "http/*.h"
    "mail/*.h"
    "misc/*.h"
    "stream/*.h" 
    "blog/*.hpp"
)

# 收集平台特定的头文件
if(WIN32)
    file(GLOB_RECURSE platform_HEADERS "os/win32/*.h")
else()
    file(GLOB_RECURSE platform_HEADERS "os/unix/*.h")
endif()

# 合并所有头文件
set(nginx_HEADERS ${common_HEADERS} ${platform_HEADERS})

# 设置平台无关的源文件
set(common_SOURCES
    # 添加模块定义文件
    ${CMAKE_SOURCE_DIR}/ngx_modules.c

    # core模块 - 平台无关部分
    core/nginx.c 
    core/ngx_log.c 
    core/ngx_palloc.c 
    core/ngx_array.c 
    core/ngx_list.c 
    core/ngx_hash.c 
    core/ngx_buf.c 
    core/ngx_queue.c 
    core/ngx_output_chain.c 
    core/ngx_string.c 
    core/ngx_parse.c 
    core/ngx_parse_time.c 
    core/ngx_inet.c 
    core/ngx_file.c 
    core/ngx_crc32.c 
    core/ngx_murmurhash.c 
    core/ngx_md5.c 
    core/ngx_sha1.c 
    core/ngx_rbtree.c 
    core/ngx_radix_tree.c 
    core/ngx_slab.c 
    core/ngx_times.c 
    core/ngx_shmtx.c 
    core/ngx_connection.c 
    core/ngx_cycle.c 
    core/ngx_spinlock.c 
    core/ngx_rwlock.c 
    core/ngx_cpuinfo.c 
    core/ngx_conf_file.c 
    core/ngx_module.c 
    core/ngx_resolver.c 
    core/ngx_open_file_cache.c 
    core/ngx_crypt.c 
    core/ngx_proxy_protocol.c 
    core/ngx_syslog.c
    
    # event模块 - 平台无关部分
    event/ngx_event.c 
    event/ngx_event_timer.c 
    event/ngx_event_posted.c 
    event/ngx_event_accept.c 
    event/ngx_event_udp.c 
    event/ngx_event_connect.c 
    event/ngx_event_pipe.c
    
    # SSL相关 - 平台无关
    event/ngx_event_openssl.c 
    event/ngx_event_openssl_cache.c
    event/ngx_event_openssl_stapling.c 
    core/ngx_regex.c 

    # HTTP模块 - 平台无关
    http/ngx_http.c 
    http/ngx_http_core_module.c 
    http/ngx_http_special_response.c 
    http/ngx_http_request.c 
    http/ngx_http_parse.c 
    http/modules/ngx_http_log_module.c 
    http/ngx_http_request_body.c 
    http/ngx_http_variables.c 
    http/ngx_http_script.c 
    http/ngx_http_upstream.c 
    http/ngx_http_upstream_round_robin.c 
    http/ngx_http_file_cache.c 
    http/ngx_http_huff_decode.c
    http/ngx_http_huff_encode.c
    http/ngx_http_write_filter_module.c 
    http/ngx_http_header_filter_module.c 
    http/modules/ngx_http_chunked_filter_module.c 
    http/modules/ngx_http_range_filter_module.c 
    http/modules/ngx_http_gzip_filter_module.c 
    http/ngx_http_postpone_filter_module.c 
    http/modules/ngx_http_ssi_filter_module.c 
    http/modules/ngx_http_charset_filter_module.c 
    http/modules/ngx_http_sub_filter_module.c 
    http/modules/ngx_http_addition_filter_module.c 
    http/modules/ngx_http_gunzip_filter_module.c 
    http/modules/ngx_http_userid_filter_module.c 
    http/modules/ngx_http_headers_filter_module.c 
    http/ngx_http_copy_filter_module.c 
    http/modules/ngx_http_not_modified_filter_module.c 
    http/modules/ngx_http_slice_filter_module.c 
    http/v2/ngx_http_v2.c 
    http/v2/ngx_http_v2_table.c 
    http/v2/ngx_http_v2_encode.c 
    http/v2/ngx_http_v2_filter_module.c 
    http/v2/ngx_http_v2_module.c 
    http/modules/ngx_http_static_module.c 
    http/modules/ngx_http_gzip_static_module.c 
    http/modules/ngx_http_dav_module.c 
    http/modules/ngx_http_autoindex_module.c 
    http/modules/ngx_http_index_module.c 
    http/modules/ngx_http_random_index_module.c 
    http/modules/ngx_http_mirror_module.c 
    http/modules/ngx_http_try_files_module.c 
    http/modules/ngx_http_auth_request_module.c 
    http/modules/ngx_http_auth_basic_module.c 
    http/modules/ngx_http_access_module.c 
    http/modules/ngx_http_limit_conn_module.c 
    http/modules/ngx_http_limit_req_module.c 
    http/modules/ngx_http_realip_module.c 
    http/modules/ngx_http_geo_module.c 
    http/modules/ngx_http_map_module.c 
    http/modules/ngx_http_split_clients_module.c 
    http/modules/ngx_http_referer_module.c 
    http/modules/ngx_http_rewrite_module.c 
    http/modules/ngx_http_ssl_module.c 
    http/modules/ngx_http_proxy_module.c 
    http/modules/ngx_http_fastcgi_module.c 
    http/modules/ngx_http_uwsgi_module.c 
    http/modules/ngx_http_scgi_module.c 
    http/modules/ngx_http_grpc_module.c 
    http/modules/ngx_http_memcached_module.c 
    http/modules/ngx_http_empty_gif_module.c 
    http/modules/ngx_http_browser_module.c 
    http/modules/ngx_http_secure_link_module.c 
    http/modules/ngx_http_flv_module.c 
    http/modules/ngx_http_mp4_module.c 
    http/modules/ngx_http_upstream_hash_module.c 
    http/modules/ngx_http_upstream_ip_hash_module.c 
    http/modules/ngx_http_upstream_least_conn_module.c 
    http/modules/ngx_http_upstream_random_module.c 
    http/modules/ngx_http_upstream_keepalive_module.c 
    http/modules/ngx_http_upstream_zone_module.c 
    http/modules/ngx_http_stub_status_module.c 

    # MAIL模块 - 平台无关
    mail/ngx_mail.c 
    mail/ngx_mail_core_module.c 
    mail/ngx_mail_handler.c 
    mail/ngx_mail_parse.c 
    mail/ngx_mail_ssl_module.c 
    mail/ngx_mail_pop3_module.c 
    mail/ngx_mail_pop3_handler.c 
    mail/ngx_mail_imap_module.c 
    mail/ngx_mail_imap_handler.c 
    mail/ngx_mail_smtp_module.c 
    mail/ngx_mail_smtp_handler.c 
    mail/ngx_mail_auth_http_module.c 
    mail/ngx_mail_proxy_module.c 
    mail/ngx_mail_realip_module.c 

    # STREAM模块 - 平台无关
    stream/ngx_stream.c 
    stream/ngx_stream_variables.c 
    stream/ngx_stream_script.c 
    stream/ngx_stream_handler.c 
    stream/ngx_stream_core_module.c 
    stream/ngx_stream_log_module.c 
    stream/ngx_stream_proxy_module.c 
    stream/ngx_stream_upstream.c 
    stream/ngx_stream_upstream_round_robin.c 
    stream/ngx_stream_write_filter_module.c 
    stream/ngx_stream_ssl_module.c 
    stream/ngx_stream_limit_conn_module.c 
    stream/ngx_stream_access_module.c 
    stream/ngx_stream_geo_module.c 
    stream/ngx_stream_map_module.c 
    stream/ngx_stream_split_clients_module.c 
    stream/ngx_stream_return_module.c
    stream/ngx_stream_upstream_hash_module.c 
    stream/ngx_stream_upstream_least_conn_module.c 
    stream/ngx_stream_upstream_random_module.c 
    stream/ngx_stream_upstream_zone_module.c 
    stream/ngx_stream_ssl_preread_module.c
    stream/ngx_stream_set_module.c
    stream/ngx_stream_pass_module.c
        
    # Blog模块
    blog/Nginx.cpp    
    blog/ngx_http_blog_module.cpp    
)

# 设置平台特定的源文件
if(WIN32)
    set(platform_SOURCES
        # Windows平台特定源文件
        os/win32/ngx_errno.c 
        os/win32/ngx_alloc.c 
        os/win32/ngx_files.c 
        os/win32/ngx_shmem.c 
        os/win32/ngx_time.c 
        os/win32/ngx_process.c 
        os/win32/ngx_thread.c 
        os/win32/ngx_socket.c 
        os/win32/ngx_wsarecv.c 
        os/win32/ngx_wsarecv_chain.c 
        os/win32/ngx_udp_wsarecv.c 
        os/win32/ngx_wsasend.c 
        os/win32/ngx_wsasend_chain.c 
        os/win32/ngx_win32_init.c 
        os/win32/ngx_user.c 
        os/win32/ngx_dlopen.c 
        os/win32/ngx_event_log.c 
        os/win32/ngx_process_cycle.c
        event/ngx_event_acceptex.c 
        event/modules/ngx_iocp_module.c 
        event/modules/ngx_win32_select_module.c 
        event/modules/ngx_win32_poll_module.c
        
        # Windows不需要的Linux特定文件将不包含在此列表
        # 比如 core/ngx_thread_pool.c
    )
else()
    set(platform_SOURCES
        # Linux/Unix平台特定源文件
        os/unix/ngx_errno.c
        os/unix/ngx_alloc.c
        os/unix/ngx_files.c
        # ... 其他Unix特定文件 ...
        core/ngx_thread_pool.c  # 只在Unix平台包含
        
        # Unix特定事件模块
        event/modules/ngx_epoll_module.c
        event/modules/ngx_kqueue_module.c
        # ... 其他Unix特定事件模块 ...
    )
endif()

# 合并所有源文件
set(nginx_SOURCES ${common_SOURCES} ${platform_SOURCES})

# 创建可执行文件
add_executable(nginx ${nginx_SOURCES})

# 使用变量从父作用域获取正确的库路径
target_link_libraries(nginx
    PRIVATE
    # 系统库
    kernel32.lib user32.lib advapi32.lib ws2_32.lib gdi32.lib crypt32.lib
    
    # 直接使用变量
    "$<$<CONFIG:Debug>:${PCRE_LIBRARIES_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${PCRE_LIBRARIES_RELEASE}>"
    "$<$<CONFIG:Debug>:${ZLIB_LIBRARIES_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${ZLIB_LIBRARIES_RELEASE}>"
    ${OPENSSL_LIBRARIES}
    
    # Boost库
    Boost::container
    Boost::exception
    Boost::utility
    Boost::tti
)

# 链接选项
target_link_options(nginx PRIVATE
    /NODEFAULTLIB:libcmt.lib
    /NODEFAULTLIB:libcmtd.lib
    /NODEFAULTLIB:msvcrt.lib
    /ignore:4217
    /ignore:4286
)

# 编译器选项
target_compile_options(nginx PRIVATE
    /W3
    /wd4996
    /wd4055
    /wd4152
    /wd4204
)

# 安装目标
install(TARGETS nginx DESTINATION ${NGINX_INSTALL_DIR})

# 自动分组源文件
AUTO_GROUP_FILES(
    TARGET nginx 
    BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${nginx_HEADERS} ${nginx_SOURCES}
)

# 设置生成位置
set_target_properties(nginx PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# 拷贝nginx原始html文件（如果有）
if(EXISTS "${CMAKE_SOURCE_DIR}/docs/html")
    add_custom_command(TARGET nginx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_SOURCE_DIR}/docs/html ${CMAKE_SOURCE_DIR}/html
        COMMENT "Copying original nginx html files..."
    )
endif()