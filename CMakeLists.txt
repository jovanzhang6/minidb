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
include_directories(${CMAKE_BINARY_DIR}/src/parser)  # Flex/Bison 生成的头文件

# ============================================================================
# Flex/Bison 配置
# ============================================================================

# 尝试查找 Flex 和 Bison
find_program(FLEX_EXECUTABLE NAMES win_flex flex)
find_program(BISON_EXECUTABLE NAMES win_bison bison)

if(FLEX_EXECUTABLE AND BISON_EXECUTABLE)
    message(STATUS "Found Flex: ${FLEX_EXECUTABLE}")
    message(STATUS "Found Bison: ${BISON_EXECUTABLE}")
    
    # 创建生成目录
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/src/parser)
    
    # Bison 生成 parser.tab.cpp 和 parser.tab.hpp
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/src/parser/parser.tab.cpp
               ${CMAKE_BINARY_DIR}/src/parser/parser.tab.hpp
        COMMAND ${BISON_EXECUTABLE} -d -o ${CMAKE_BINARY_DIR}/src/parser/parser.tab.cpp
                ${CMAKE_SOURCE_DIR}/src/parser/parser.y
        DEPENDS ${CMAKE_SOURCE_DIR}/src/parser/parser.y
        COMMENT "Generating parser with Bison"
        VERBATIM
    )
    
    # Flex 生成 lexer.yy.cpp
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/src/parser/lexer.yy.cpp
        COMMAND ${FLEX_EXECUTABLE} -o ${CMAKE_BINARY_DIR}/src/parser/lexer.yy.cpp
                ${CMAKE_SOURCE_DIR}/src/parser/lexer.l
        DEPENDS ${CMAKE_SOURCE_DIR}/src/parser/lexer.l
                ${CMAKE_BINARY_DIR}/src/parser/parser.tab.hpp
        COMMENT "Generating lexer with Flex"
        VERBATIM
    )
    
    # Parser 库（使用 Flex/Bison 生成的代码）
    set(PARSER_SOURCES
        ${CMAKE_BINARY_DIR}/src/parser/parser.tab.cpp
        ${CMAKE_BINARY_DIR}/src/parser/lexer.yy.cpp
        ${CMAKE_SOURCE_DIR}/src/parser/ast.cpp
        ${CMAKE_SOURCE_DIR}/src/parser/parser.cpp
    )
    
    set(USE_FLEX_BISON TRUE)
else()
    message(WARNING "Flex/Bison not found, using hand-written parser")
    
    # 使用手写 Parser 的源文件
    set(PARSER_SOURCES
        ${CMAKE_SOURCE_DIR}/src/parser/ast.cpp
        ${CMAKE_SOURCE_DIR}/src/parser/lexer.cpp
        ${CMAKE_SOURCE_DIR}/src/parser/parser_handwritten.cpp
    )
    
    set(USE_FLEX_BISON FALSE)
endif()

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

# 核心库（不包含 Parser）
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

# Parser 库
add_library(minidb_parser STATIC ${PARSER_SOURCES})
target_link_libraries(minidb_parser minidb_core)

# MSVC 特殊处理
if(MSVC)
    # 禁用生成代码的一些警告
    set_source_files_properties(
        ${CMAKE_BINARY_DIR}/src/parser/parser.tab.cpp
        ${CMAKE_BINARY_DIR}/src/parser/lexer.yy.cpp
        PROPERTIES COMPILE_FLAGS "/wd4005 /wd4127 /wd4244 /wd4267"
    )
endif()

# 主程序
add_executable(minidb src/shell/main.cpp)
target_link_libraries(minidb minidb_parser minidb_core)

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

# 页面布局测试
add_executable(page_test test/page_test.cpp)
target_link_libraries(page_test minidb_core)
add_test(NAME PageTest COMMAND page_test)

# 记录格式测试
add_executable(record_test test/record_test.cpp)
target_link_libraries(record_test minidb_core)
add_test(NAME RecordTest COMMAND record_test)

# B-tree测试
add_executable(btree_test test/btree_test.cpp)
target_link_libraries(btree_test minidb_core)
add_test(NAME BTreeTest COMMAND btree_test)

# Catalog测试
add_executable(catalog_test test/catalog_test.cpp)
target_link_libraries(catalog_test minidb_core)
add_test(NAME CatalogTest COMMAND catalog_test)

# Parser测试
add_executable(parser_test test/parser_test.cpp)
target_link_libraries(parser_test minidb_parser minidb_core)
add_test(NAME ParserTest COMMAND parser_test)

# Executor测试
add_executable(executor_test test/executor_test.cpp)
target_link_libraries(executor_test minidb_parser minidb_core)
add_test(NAME ExecutorTest COMMAND executor_test)

# Transaction测试
add_executable(txn_test test/txn_test.cpp)
target_link_libraries(txn_test minidb_core)
add_test(NAME TxnTest COMMAND txn_test)

# 集成测试（演示底层功能）
add_executable(integration_test test/integration_test.cpp)
target_link_libraries(integration_test minidb_core)

# Demo测试（简化版，英文输出）
add_executable(demo_test test/demo_test.cpp)
target_link_libraries(demo_test minidb_core)
