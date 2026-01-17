/**
 * @file config.h
 * @brief Global configuration constants
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace minidb {

// Buffer pool configuration
constexpr size_t DEFAULT_BUFFER_POOL_SIZE = 100;

// B-tree configuration
constexpr uint32_t BTREE_LEAF_HEADER_SIZE = 8;
constexpr uint32_t BTREE_INTERIOR_HEADER_SIZE = 12;
constexpr uint32_t CELL_POINTER_SIZE = 2;

// Overflow threshold
constexpr uint32_t MAX_PAYLOAD_IN_LEAF = 4096 - 35;

// Freeblock minimum size
constexpr uint32_t MIN_FREEBLOCK_SIZE = 4;

// Defragment threshold
constexpr uint32_t DEFRAG_THRESHOLD = 60;

// Transaction configuration
constexpr uint32_t MAX_CONCURRENT_TXN = 16;

// Journal magic number
constexpr uint64_t JOURNAL_MAGIC = 0xD9D505F920A163D7ULL;

// System table names
constexpr const char* SYS_SCHEMA_TABLE = "sys_schema";
constexpr const char* SYS_USERS_TABLE = "sys_users";
constexpr const char* SYS_PRIVILEGES_TABLE = "sys_privileges";

} // namespace minidb
