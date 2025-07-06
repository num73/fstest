cmake_minimum_required(VERSION 3.10)

# 设置项目名称和版本
project(iotest VERSION 1.0.0 LANGUAGES C)

# 设置C标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 添加编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -Wall -Wextra)
else()
    add_compile_options(-O2)
endif()

# 包含子目录
add_subdirectory(fstest)