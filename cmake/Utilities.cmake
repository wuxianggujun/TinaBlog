# Utilities.cmake - CMake工具函数集
# ===============================================================

# 查找库文件的通用函数
function(find_library_file OUT_VAR NAME POSSIBLE_PATHS)
    foreach(PATH ${POSSIBLE_PATHS})
        if(EXISTS "${PATH}")
            set(${OUT_VAR} "${PATH}" CACHE FILEPATH "${NAME} library file" FORCE)
            message(STATUS "找到${NAME}: ${PATH}")
            return()
        endif()
    endforeach()
    
    message(WARNING "未找到${NAME}，请检查路径或手动设置")
    set(${OUT_VAR} "" CACHE FILEPATH "${NAME} library file" FORCE)
endfunction()

# 创建并配置一个导入的库目标
function(create_imported_library TARGET_NAME LIB_TYPE LIB_PATH INCLUDE_DIRS)
    if(NOT TARGET ${TARGET_NAME})
        add_library(${TARGET_NAME} ${LIB_TYPE} IMPORTED)
        if(EXISTS "${LIB_PATH}")
            set_target_properties(${TARGET_NAME} PROPERTIES
                IMPORTED_LOCATION "${LIB_PATH}"
                INTERFACE_INCLUDE_DIRECTORIES "${INCLUDE_DIRS}"
            )
            message(STATUS "创建导入的库: ${TARGET_NAME}, 路径: ${LIB_PATH}")
        else()
            message(WARNING "无法创建库 ${TARGET_NAME}，文件不存在: ${LIB_PATH}")
        endif()
    endif()
endfunction()

# 配置编译选项的函数
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

# 将源文件自动分组到IDE中的函数
function(auto_group_sources TARGET_NAME BASE_DIR)
    get_target_property(TARGET_SOURCES ${TARGET_NAME} SOURCES)
    
    foreach(SOURCE ${TARGET_SOURCES})
        get_filename_component(SOURCE_PATH "${SOURCE}" REALPATH)
        file(RELATIVE_PATH REL_PATH "${BASE_DIR}" "${SOURCE_PATH}")
        get_filename_component(SOURCE_DIR "${REL_PATH}" DIRECTORY)
        string(REPLACE "/" "\\" IDE_SOURCE_GROUP "${SOURCE_DIR}")
        source_group("${IDE_SOURCE_GROUP}" FILES "${SOURCE}")
    endforeach()
endfunction() 