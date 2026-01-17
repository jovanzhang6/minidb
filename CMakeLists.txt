cmake_minimum_required(VERSION 3.16)
project(MiniDB VERSION 1.0.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/src)

# 源文件收集
file(GLOB_RECURSE COMMON_SOURCES "src/common/*.cpp")
file(GLOB_RECURSE STORAGE_SOURCES "src/storage/*.cpp")
file(GLOB_RECURSE BUFFER_SOURCES "src/buffer/*.cpp")
file(GLOB_RECURSE PAGE_SOURCES "src/page/*.cpp")
file(GLOB_RECURSE BTREE_SOURCES "src/btree/*.cpp")
file(GLOB_RECURSE RECORD_SOURCES "src/record/*.cpp")
file(GLOB_RECURSE CATALOG_SOURCES "src/catalog/*.cpp")
file(GLOB_RECURSE EXECUTOR_SOURCES "src/executor/*.cpp")
file(GLOB_RECURSE TXN_SOURCES "src/txn/*.cpp")

# 核心库
add_library(minidb_core STATIC
    ${COMMON_SOURCES}
    ${STORAGE_SOURCES}
    ${BUFFER_SOURCES}
    ${PAGE_SOURCES}
    ${BTREE_SOURCES}
    ${RECORD_SOURCES}
    ${CATALOG_SOURCES}
    ${EXECUTOR_SOURCES}
    ${TXN_SOURCES}
)

# 主程序
add_executable(minidb src/shell/main.cpp)
target_link_libraries(minidb minidb_core)

# 测试
enable_testing()

# 存储层测试
add_executable(storage_test test/storage_test.cpp)
target_link_libraries(storage_test minidb_core)
add_test(NAME StorageTest COMMAND storage_test)

# 缓冲池测试
add_executable(buffer_test test/buffer_test.cpp)
target_link_libraries(buffer_test minidb_core)
add_test(NAME BufferTest COMMAND buffer_test)

# 记录格式测试
add_executable(record_test test/record_test.cpp)
target_link_libraries(record_test minidb_core)
add_test(NAME RecordTest COMMAND record_test)

# B-tree测试
add_executable(btree_test test/btree_test.cpp)
target_link_libraries(btree_test minidb_core)
add_test(NAME BTreeTest COMMAND btree_test)
