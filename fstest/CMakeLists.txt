# raw_dev子项目的CMakeLists.txt

# 设置源文件
set(FS_TEST_SOURCES
    fstest.c
    tools.c
    mmtest.c
)

# 创建可执行文件
add_executable(fstest ${FS_TEST_SOURCES})

# 设置输出目录
set_target_properties(fstest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 如果需要链接其他库，可以在这里添加
# target_link_libraries(fstest
#     # 在这里添加需要链接的库
# )

# 设置包含目录（如果有头文件）
# target_include_directories(fstest PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
# )