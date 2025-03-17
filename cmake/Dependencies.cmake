# Dependencies.cmake - 统一的依赖库管理
# ===============================================================
# 该文件集中管理所有外部依赖库，区分本地库和vcpkg库

# 定义区分本地库和vcpkg库的集合
set(VCPKG_LIBS
    "PCRE2"     # 正则表达式库
    "ZLIB"      # 压缩库
    "OpenSSL"   # SSL库
    "MySQL-Connector-C++"  # MySQL Connector C++
    "nlohmann-json"  # JSON库
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

# 添加版本检查函数
function(check_dependency_versions)
    # 设置最低版本要求
    set(MIN_PCRE2_VERSION "10.40")
    set(MIN_ZLIB_VERSION "1.2.13")
    set(MIN_OPENSSL_VERSION "3.0.0")
    
    # 检查PCRE2版本
    if(pcre2_VERSION VERSION_LESS ${MIN_PCRE2_VERSION})
        message(WARNING "PCRE2版本过低: ${pcre2_VERSION}，建议至少使用 ${MIN_PCRE2_VERSION}")
    endif()
    
    # 检查ZLIB版本
    if(ZLIB_VERSION_STRING VERSION_LESS ${MIN_ZLIB_VERSION})
        message(WARNING "ZLIB版本过低: ${ZLIB_VERSION_STRING}，建议至少使用 ${MIN_ZLIB_VERSION}")
    endif()
    
    # 检查OpenSSL版本
    if(OPENSSL_VERSION VERSION_LESS ${MIN_OPENSSL_VERSION})
        message(WARNING "OpenSSL版本过低: ${OPENSSL_VERSION}，建议至少使用 ${MIN_OPENSSL_VERSION}")
    endif()
    
    message(STATUS "依赖库版本检查完成")
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
        elseif(LIB STREQUAL "nlohmann-json")
            # 设置nlohmann-json不使用隐式转换
            set(nlohmann-json_IMPLICIT_CONVERSIONS OFF)
            find_package(nlohmann_json CONFIG REQUIRED)
            message(STATUS "找到nlohmann-json库")
        endif()
    endforeach()
    
    # 查找所有本地安装的库
    foreach(LIB ${LOCAL_LIBS})
        if(LIB STREQUAL "MySQL")
            find_mysql_server()
        endif()
    endforeach()
    
    # 执行版本检查
    check_dependency_versions()
    
    message(STATUS "----- 依赖库查找结果 -----")
    message(STATUS "PCRE2: ${pcre2_VERSION}")
    message(STATUS "ZLIB: ${ZLIB_VERSION_STRING}")
    message(STATUS "OpenSSL: ${OPENSSL_VERSION}")
    message(STATUS "MySQL Server: ${MYSQL_SERVER_DIR}")
    message(STATUS "MySQL Connector C++: ${MYSQL_CONNECTOR_INCLUDE_DIRS}")
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
    # 设置DLL输出目录 - 改用构建输出目录
    set(DLL_OUTPUT_DIR "$<TARGET_FILE_DIR:${TARGET_NAME}>")
    
    # 添加复制依赖库到输出目录的逻辑
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        # 复制MySQL Server DLL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYSQL_SERVER_DIR}/lib/libmysql.dll"
            "${DLL_OUTPUT_DIR}/libmysql.dll"
        # 复制vcpkg库文件
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/zlib1.dll"
            "${DLL_OUTPUT_DIR}/zlib1.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/pcre2-8.dll"
            "${DLL_OUTPUT_DIR}/pcre2-8.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libcrypto-3-x64.dll" 
            "${DLL_OUTPUT_DIR}/libcrypto-3-x64.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libssl-3-x64.dll"
            "${DLL_OUTPUT_DIR}/libssl-3-x64.dll"
        COMMENT "复制运行时依赖DLL到输出目录..."
    )
    
    # 查找MySQL Connector C++ DLL
    file(GLOB MYSQL_CPP_DLL_FILES
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/mysqlcppconn*.dll"
    )
    
    foreach(DLL_FILE ${MYSQL_CPP_DLL_FILES})
        get_filename_component(DLL_FILENAME ${DLL_FILE} NAME)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DLL_FILE}
                "${DLL_OUTPUT_DIR}/${DLL_FILENAME}"
            COMMENT "复制MySQL Connector C++ DLL: ${DLL_FILENAME}"
        )
    endforeach()
    
    # 复制Protobuf DLL (MySQL Connector C++依赖库)
    file(GLOB PROTOBUF_DLL_FILES
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libprotobuf-lite.dll"
    )
    
    foreach(DLL_FILE ${PROTOBUF_DLL_FILES})
        get_filename_component(DLL_FILENAME ${DLL_FILE} NAME)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DLL_FILE}
                "${DLL_OUTPUT_DIR}/${DLL_FILENAME}"
            COMMENT "复制Protobuf DLL (MySQL Connector C++依赖): ${DLL_FILENAME}"
        )
    endforeach()
    
    # 复制LZ4 DLL (另一个MySQL Connector C++依赖)
    if(EXISTS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/lz4.dll")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/lz4.dll"
                "${DLL_OUTPUT_DIR}/lz4.dll"
            COMMENT "复制LZ4 DLL (MySQL Connector C++依赖)"
        )
    endif()
    
    # 复制其他可能的依赖DLL
    set(POSSIBLE_DEPS 
        "zstd.dll"      # ZSTD压缩库
        "xxhash.dll"    # Hash库
        "fmt.dll"       # 格式化库
    )
    
    foreach(DEP_DLL ${POSSIBLE_DEPS})
        if(EXISTS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/${DEP_DLL}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/${DEP_DLL}"
                    "${DLL_OUTPUT_DIR}/${DEP_DLL}"
                COMMENT "复制${DEP_DLL} (可能的MySQL Connector C++依赖)"
            )
        endif()
    endforeach()
    
    # 复制vcpkg bin目录中所有包含"mysql"的DLL
    # 这是为了确保捕获所有MySQL相关的DLL
    file(GLOB MYSQL_RELATED_DLLS
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/*mysql*.dll"
    )
    
    foreach(DLL_FILE ${MYSQL_RELATED_DLLS})
        get_filename_component(DLL_FILENAME ${DLL_FILE} NAME)
        if(NOT "${DLL_FILENAME}" MATCHES "mysqlcppconn") # 避免重复复制已处理的文件
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${DLL_FILE}
                    "${DLL_OUTPUT_DIR}/${DLL_FILENAME}"
                COMMENT "复制MySQL相关DLL: ${DLL_FILENAME}"
            )
        endif()
    endforeach()
    
    message(STATUS "已配置DLL复制到目录: ${DLL_OUTPUT_DIR}")
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
        elseif(LIB STREQUAL "nlohmann-json")
            target_link_libraries(${TARGET_NAME} PRIVATE 
                nlohmann_json::nlohmann_json
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