# third_party/CMakeLists.txt
# 第三方库的集中配置入口点

# 所有第三方库的配置都集中在ThirdPartyLibs.cmake中
# 这个文件只是一个转发文件，确保所有库的配置都是集中管理的

message(STATUS "加载第三方库配置 - 使用统一的ThirdPartyLibs.cmake")

# 如果有特殊的编译选项设置，应该在ThirdPartyLibs.cmake中设置
# 这里不再重复设置全局编译选项

# 设置子项目输出位置
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 第三方库的实际配置由ThirdPartyLibs.cmake完成