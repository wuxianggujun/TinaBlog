# MySQL.cmake - MySQL相关库配置
# ===============================================================
# 本文件负责MySQL Server和MySQL Connector C++的配置

# MySQL Server配置函数
function(configure_mysql_server)
    # 选项控制是否禁用MySQL
    option(MYSQL_DISABLE "禁用MySQL功能" OFF)
    
    # 如果MySQL功能已禁用，直接返回
    if(MYSQL_DISABLE)
        message(STATUS "MySQL功能已被禁用")
        add_definitions(-DHAVE_MYSQL=0)
        return()
    endif()
    
    # 使用本地安装的MySQL
    message(STATUS "配置本地安装的MySQL Server...")
    
    # 设置MySQL相关路径选项 - 明确区分不同目录的用途
    set(MYSQL_SERVER_DIR "D:/Program Files/MySQL/MySQL Server 9.2" CACHE PATH "MySQL Server安装路径")
    
    # 导出目录变量到父作用域
    set(MYSQL_SERVER_DIR ${MYSQL_SERVER_DIR} PARENT_SCOPE)
    
    # 设置MySQL Server相关变量
    set(MySQL_INCLUDE_DIRS 
        "${MYSQL_SERVER_DIR}/include"           # MySQL Server头文件
        CACHE PATH "MySQL包含目录"
    )
    
    # 设置MySQL Server的库文件路径
    set(MySQL_LIBRARIES 
        "${MYSQL_SERVER_DIR}/lib/libmysql.lib"  # MySQL Server库文件
        CACHE PATH "MySQL库文件"
    )
    
    # 检查MySQL Server DLL
    set(MYSQL_DLL_PATH "${MYSQL_SERVER_DIR}/lib/libmysql.dll")
    if(EXISTS "${MYSQL_DLL_PATH}")
        set(MySQL_DLL "${MYSQL_DLL_PATH}" CACHE PATH "MySQL DLL文件" FORCE)
        message(STATUS "找到MySQL DLL文件: ${MYSQL_DLL_PATH}")
    else()
        message(WARNING "MySQL DLL文件不存在: ${MYSQL_DLL_PATH}")
    endif()
    
    # 检查MySQL头文件和库是否存在
    set(MYSQL_HEADER_FOUND TRUE)
    set(MYSQL_SERVER_INCLUDE "${MYSQL_SERVER_DIR}/include")
    
    if(NOT EXISTS "${MYSQL_SERVER_INCLUDE}")
        message(WARNING "MySQL Server头文件目录不存在: ${MYSQL_SERVER_INCLUDE}")
        set(MYSQL_HEADER_FOUND FALSE)
    endif()
    
    set(MYSQL_LIB_PATH "${MYSQL_SERVER_DIR}/lib/libmysql.lib")
    if(NOT EXISTS "${MYSQL_LIB_PATH}")
        message(WARNING "MySQL库文件不存在: ${MYSQL_LIB_PATH}")
        set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has not been found" FORCE)
    elseif(NOT MYSQL_HEADER_FOUND)
        message(WARNING "MySQL某些头文件目录不存在，请检查路径")
        set(MYSQL_FOUND FALSE CACHE BOOL "MySQL has not been found" FORCE)
    else()
        set(MYSQL_FOUND TRUE CACHE BOOL "MySQL has been found" FORCE)
        add_definitions(-DHAVE_MYSQL=1)
        message(STATUS "MySQL库配置成功")
    endif()
    
    # 导出变量到父作用域
    set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDE_DIRS} PARENT_SCOPE)
    set(MySQL_LIBRARIES ${MySQL_LIBRARIES} PARENT_SCOPE)
    set(MySQL_DLL ${MySQL_DLL} PARENT_SCOPE)
    set(MYSQL_FOUND ${MYSQL_FOUND} PARENT_SCOPE)
endfunction()

# MySQL Connector C++配置函数
function(configure_mysql_connector)
    message(STATUS "配置MySQL Connector C++...")
    
    # 通过vcpkg查找MySQL Connector C++
    find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)
    
    if(unofficial-mysql-connector-cpp_FOUND)
        message(STATUS "已通过vcpkg找到MySQL Connector C++")
        
        # 设置头文件目录
        set(MYSQL_CONNECTOR_INCLUDE_DIRS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/mysql-cppconn-8" PARENT_SCOPE)
        set(MYSQL_CONNECTOR_LIBRARIES unofficial::mysql-connector-cpp::connector PARENT_SCOPE)
        
        message(STATUS "MySQL Connector C++已成功配置(vcpkg)")
        message(STATUS "MySQL Connector C++头文件: ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/mysql-cppconn-8")
        message(STATUS "MySQL Connector C++库: unofficial::mysql-connector-cpp::connector")
    else()
        message(FATAL_ERROR "未找到MySQL Connector C++。请确保通过vcpkg安装: vcpkg install mysql-connector-cpp:x64-windows")
    endif()
endfunction()

# 主函数 - 配置MySQL所有相关项
function(configure_mysql_all)
    # 配置MySQL Server
    configure_mysql_server()
    
    # 配置MySQL Connector C++
    configure_mysql_connector()
    
    # 打印配置结果
    message(STATUS "==== MySQL配置结果 ====")
    message(STATUS "MySQL Server: ${MYSQL_FOUND}")
    message(STATUS "MySQL Connector: ${MYSQL_CONNECTOR_LIBRARIES}")
    message(STATUS "MySQL Connector头文件: ${MYSQL_CONNECTOR_INCLUDE_DIRS}")
    message(STATUS "========================")
endfunction()

# 自动调用主函数
configure_mysql_all() 