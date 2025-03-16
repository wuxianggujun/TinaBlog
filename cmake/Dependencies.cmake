# Dependencies.cmake - 统一的依赖库管理
# ===============================================================
# 该文件集中管理所有外部依赖库，区分本地库和vcpkg库

# 定义区分本地库和vcpkg库的集合
set(VCPKG_LIBS
    "PCRE2"     # 正则表达式库
    "ZLIB"      # 压缩库
    "OpenSSL"   # SSL库
    "MySQL-Connector-C++"  # MySQL Connector C++
)

set(LOCAL_LIBS
    "MySQL"     # MySQL Server本地安装
)

# 配置编译选项的函数 - 从Utilities.cmake整合
function(configure_compiler_options TARGET_NAME)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE
            /W3
            /wd4996  # 禁止"deprecated"警告
            /wd4055  # 禁止类型转换警告 
            /wd4152  # 禁止函数指针转换警告
            /wd4204  # 非标准扩展警告
        )
        
        target_link_options(${TARGET_NAME} PRIVATE
            /NODEFAULTLIB:libcmt.lib
            /NODEFAULTLIB:libcmtd.lib
            /NODEFAULTLIB:msvcrt.lib
            /ignore:4217
            /ignore:4286
        )
    else()
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wno-unused-parameter
        )
    endif()
endfunction()

# 统一的依赖查找函数
function(find_all_dependencies)
    message(STATUS "----- 开始查找所有依赖库 -----")
    
    # 循环查找所有vcpkg安装的库
    foreach(LIB ${VCPKG_LIBS})
        if(LIB STREQUAL "PCRE2")
            find_package(pcre2 CONFIG REQUIRED)
            message(STATUS "找到PCRE2: ${pcre2_VERSION}")
        elseif(LIB STREQUAL "ZLIB")
            find_package(ZLIB REQUIRED)
            message(STATUS "找到ZLIB: ${ZLIB_VERSION_STRING}")
        elseif(LIB STREQUAL "OpenSSL")
            find_package(OpenSSL REQUIRED)
            message(STATUS "找到OpenSSL: ${OPENSSL_VERSION}")
        elseif(LIB STREQUAL "MySQL-Connector-C++")
            find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)
            set(MYSQL_CONNECTOR_INCLUDE_DIRS 
                "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/mysql-cppconn-8"
                CACHE PATH "MySQL Connector C++包含目录" FORCE
            )
            message(STATUS "找到MySQL Connector C++")
            # 导出变量到父作用域
            set(MYSQL_CONNECTOR_INCLUDE_DIRS ${MYSQL_CONNECTOR_INCLUDE_DIRS} PARENT_SCOPE)
        endif()
    endforeach()
    
    # 查找所有本地安装的库
    foreach(LIB ${LOCAL_LIBS})
        if(LIB STREQUAL "MySQL")
            find_mysql_server()
        endif()
    endforeach()
    
    message(STATUS "----- 依赖库查找结果 -----")
    message(STATUS "---------------------------")
endfunction()

# MySQL Server配置函数
function(find_mysql_server)
    # 设置MySQL相关路径选项
    set(MYSQL_SERVER_DIR "D:/Program Files/MySQL/MySQL Server 9.2" CACHE PATH "MySQL Server安装路径")
    
    # 导出目录变量到父作用域
    set(MYSQL_SERVER_DIR ${MYSQL_SERVER_DIR} PARENT_SCOPE)
    
    # 设置MySQL Server相关变量
    set(MySQL_INCLUDE_DIRS 
        "${MYSQL_SERVER_DIR}/include"
        CACHE PATH "MySQL包含目录"
    )
    
    # 设置MySQL Server的库文件路径
    set(MySQL_LIBRARIES 
        "${MYSQL_SERVER_DIR}/lib/libmysql.lib"
        CACHE PATH "MySQL库文件"
    )
    
    # 检查MySQL头文件和库是否存在
    if(NOT EXISTS "${MYSQL_SERVER_DIR}/include")
        message(WARNING "MySQL Server头文件目录不存在: ${MYSQL_SERVER_DIR}/include")
        set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has not been found" FORCE)
    elseif(NOT EXISTS "${MYSQL_SERVER_DIR}/lib/libmysql.lib")
        message(WARNING "MySQL库文件不存在: ${MYSQL_SERVER_DIR}/lib/libmysql.lib")
        set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has not been found" FORCE)
    else()
        set(MYSQL_FOUND TRUE CACHE BOOL "MySQL has been found" FORCE)
        message(STATUS "MySQL Server库配置成功")
    endif()
    
    # 导出变量到父作用域
    set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDE_DIRS} PARENT_SCOPE)
    set(MySQL_LIBRARIES ${MySQL_LIBRARIES} PARENT_SCOPE)
    set(MYSQL_FOUND ${MYSQL_FOUND} PARENT_SCOPE)
endfunction()

# 配置运行时DLL复制
function(configure_dll_dependencies TARGET_NAME)
    # 创建通用的DLL复制函数
    function(copy_dll SOURCE_PATH TARGET_NAME FILENAME COMMENT_TEXT)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOURCE_PATH}"
                "${CMAKE_SOURCE_DIR}/${FILENAME}"
            COMMENT "${COMMENT_TEXT}: ${FILENAME}"
        )
    endfunction()
    
    # 复制单个DLL文件的函数
    function(copy_single_dll TARGET_NAME DLL_PATH DLL_CATEGORY)
        if(EXISTS "${DLL_PATH}")
            get_filename_component(DLL_FILENAME ${DLL_PATH} NAME)
            copy_dll("${DLL_PATH}" ${TARGET_NAME} "${DLL_FILENAME}" "复制${DLL_CATEGORY}依赖")
        endif()
    endfunction()
    
    # 复制目录中匹配模式的所有DLL
    function(copy_dll_pattern TARGET_NAME DLL_DIR PATTERN DLL_CATEGORY EXCLUDE_PATTERN)
        file(GLOB DLL_FILES "${DLL_DIR}/${PATTERN}")
        foreach(DLL_FILE ${DLL_FILES})
            get_filename_component(DLL_FILENAME ${DLL_FILE} NAME)
            if("${EXCLUDE_PATTERN}" STREQUAL "" OR NOT "${DLL_FILENAME}" MATCHES "${EXCLUDE_PATTERN}")
                copy_dll("${DLL_FILE}" ${TARGET_NAME} "${DLL_FILENAME}" "复制${DLL_CATEGORY}依赖")
            endif()
        endforeach()
    endfunction()
    
    # 1. 复制基本依赖库
    set(BASIC_DLLS
        "${MYSQL_SERVER_DIR}/lib/libmysql.dll|MySQL Server"
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/zlib1.dll|ZLIB压缩库"
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/pcre2-8.dll|PCRE2正则表达式库"
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libcrypto-3-x64.dll|OpenSSL加密库"
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libssl-3-x64.dll|OpenSSL SSL库"
    )
    
    foreach(DLL_ENTRY ${BASIC_DLLS})
        string(REPLACE "|" ";" DLL_PARTS ${DLL_ENTRY})
        list(GET DLL_PARTS 0 DLL_PATH)
        list(GET DLL_PARTS 1 DLL_CATEGORY)
        copy_single_dll(${TARGET_NAME} "${DLL_PATH}" "${DLL_CATEGORY}")
    endforeach()
    
    # 2. 复制MySQL Connector C++ DLL和其依赖
    # MySQL Connector C++主要库
    copy_dll_pattern(${TARGET_NAME} "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin" "mysqlcppconn*.dll" "MySQL Connector C++" "")
    
    # Protobuf依赖
    copy_dll_pattern(${TARGET_NAME} "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin" "libprotobuf*.dll" "Protobuf (MySQL Connector C++依赖)" "")
    
    # 其他MySQL相关DLL
    copy_dll_pattern(${TARGET_NAME} "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin" "*mysql*.dll" "MySQL相关" "mysqlcppconn")
    
    # 3. 复制其他可能的依赖DLL
    set(POSSIBLE_DEPS 
        "lz4.dll|LZ4压缩库"
        "zstd.dll|ZSTD压缩库"
        "xxhash.dll|Hash库"
        "fmt.dll|格式化库"
    )
    
    foreach(DEP_ENTRY ${POSSIBLE_DEPS})
        string(REPLACE "|" ";" DEP_PARTS ${DEP_ENTRY})
        list(GET DEP_PARTS 0 DEP_FILENAME)
        list(GET DEP_PARTS 1 DEP_CATEGORY)
        set(DEP_PATH "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/${DEP_FILENAME}")
        copy_single_dll(${TARGET_NAME} "${DEP_PATH}" "${DEP_CATEGORY}")
    endforeach()
    
    message(STATUS "已配置所有依赖DLL的复制")
endfunction()

# 链接所有依赖库到目标
function(link_all_dependencies TARGET_NAME)
    # Windows系统库
    if(WIN32)
        set(SYSTEM_LIBS
            kernel32.lib
            user32.lib
            advapi32.lib
            ws2_32.lib
            gdi32.lib
            crypt32.lib
        )
        target_link_libraries(${TARGET_NAME} PRIVATE ${SYSTEM_LIBS})
    endif()
    
    # 链接所有VCPKG库
    foreach(LIB ${VCPKG_LIBS})
        if(LIB STREQUAL "PCRE2")
            target_link_libraries(${TARGET_NAME} PRIVATE PCRE2::8BIT)
            target_compile_definitions(${TARGET_NAME} PRIVATE 
                NGX_PCRE2=1
                NGX_HAVE_PCRE=1
            )
        elseif(LIB STREQUAL "ZLIB")
            target_link_libraries(${TARGET_NAME} PRIVATE ZLIB::ZLIB)
        elseif(LIB STREQUAL "OpenSSL")
            target_link_libraries(${TARGET_NAME} PRIVATE 
                OpenSSL::SSL 
                OpenSSL::Crypto
            )
        elseif(LIB STREQUAL "MySQL-Connector-C++")
            target_link_libraries(${TARGET_NAME} PRIVATE 
                unofficial::mysql-connector-cpp::connector
            )
        endif()
    endforeach()
    
    # 链接所有本地库
    foreach(LIB ${LOCAL_LIBS})
        if(LIB STREQUAL "MySQL")
            if(MYSQL_FOUND)
                target_link_libraries(${TARGET_NAME} PRIVATE ${MySQL_LIBRARIES})
            endif()
        endif()
    endforeach()
    
    message(STATUS "已链接所有依赖库到目标: ${TARGET_NAME}")
endfunction()

# 设置全局编译选项
function(configure_global_options)
    if(MSVC)
        # 统一所有库使用动态运行时库/MD
        foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE)
            if(${flag_var} MATCHES "/MT")
                string(REGEX REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
                set(${flag_var} "${${flag_var}}" PARENT_SCOPE)
            endif()
        endforeach()
        
        # 确保在Debug模式中使用/MDd
        if(NOT CMAKE_C_FLAGS_DEBUG MATCHES "/MDd")
            set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd" PARENT_SCOPE)
        endif()
        if(NOT CMAKE_CXX_FLAGS_DEBUG MATCHES "/MDd")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd" PARENT_SCOPE)
        endif()
    endif()
endfunction()

# 初始化所有依赖配置
configure_global_options()
find_all_dependencies() 