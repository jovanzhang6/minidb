/**
 * @file types.h
 * @brief Common type definitions
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>
#include <vector>
#include <optional>

namespace minidb {

// Page constants
constexpr uint32_t PAGE_SIZE = 4096;
constexpr uint32_t DB_HEADER_SIZE = 100;
constexpr uint32_t INVALID_PAGE_ID = 0;

// Page types
enum class PageType : uint8_t {
    PAGE_INVALID = 0x00,
    INDEX_INTERIOR = 0x02,
    TABLE_INTERIOR = 0x05,
    INDEX_LEAF = 0x0a,
    TABLE_LEAF = 0x0d,
    PAGE_FREELIST_TRUNK = 0x01,
    PAGE_OVERFLOW = 0x03,
};

// Type aliases
using page_id_t = uint32_t;
using rowid_t = int64_t;
using frame_id_t = int32_t;
using txn_id_t = uint32_t;

constexpr frame_id_t INVALID_FRAME_ID = -1;
constexpr txn_id_t INVALID_TXN_ID = 0;

// Data types
enum class DataType : uint8_t {
    INVALID = 0,
    INT = 1,
    FLOAT = 2,
    TEXT = 3,
};

// Serial types for record serialization
enum SerialType : uint32_t {
    SERIAL_NULL = 0,
    SERIAL_INT8 = 1,
    SERIAL_INT16 = 2,
    SERIAL_INT24 = 3,
    SERIAL_INT32 = 4,
    SERIAL_INT48 = 5,
    SERIAL_INT64 = 6,
    SERIAL_FLOAT64 = 7,
    SERIAL_ZERO = 8,
    SERIAL_ONE = 9,
};

// Magic number (16 bytes)
inline constexpr char DB_MAGIC[16] = {'M','i','n','i','D','B',' ','f','o','r','m','a','t',' ','1','\0'};

// Error codes
enum class ErrorCode : int {
    SUCCESS = 0,
    IO_ERROR = -1,
    PAGE_NOT_FOUND = -2,
    NO_FREE_PAGE = -3,
    BUFFER_FULL = -4,
    INVALID_PAGE_TYPE = -5,
    KEY_NOT_FOUND = -6,
    DUPLICATE_KEY = -7,
    TABLE_NOT_FOUND = -8,
    COLUMN_NOT_FOUND = -9,
    TYPE_MISMATCH = -10,
    SYNTAX_ERROR = -11,
    PERMISSION_DENIED = -12,
    TXN_ABORT = -13,
    CORRUPTED_DATA = -14,
    TRANSACTION_IN_PROGRESS = -15,  // 已有活动事务
    NO_TRANSACTION = -16,           // 无活动事务
    INDEX_NOT_FOUND = -17,          // 索引不存在
    INDEX_EXISTS = -18,             // 索引已存在
};

// Value class: stores a single field value
class Value {
public:
    Value() : type_(DataType::INVALID) {}
    
    explicit Value(int64_t i) : type_(DataType::INT), int_val_(i) {}
    explicit Value(double f) : type_(DataType::FLOAT), float_val_(f) {}
    explicit Value(const std::string& s) : type_(DataType::TEXT), str_val_(s) {}
    explicit Value(std::string&& s) : type_(DataType::TEXT), str_val_(std::move(s)) {}
    
    static Value Null() { return Value(); }
    
    bool IsNull() const { return type_ == DataType::INVALID; }
    DataType GetType() const { return type_; }
    
    int64_t GetInt() const { return int_val_; }
    double GetFloat() const { return float_val_; }
    const std::string& GetText() const { return str_val_; }
    
    bool operator==(const Value& other) const;
    bool operator<(const Value& other) const;
    bool operator>(const Value& other) const;
    bool operator<=(const Value& other) const;
    bool operator>=(const Value& other) const;
    bool operator!=(const Value& other) const;
    
    std::string ToString() const;

private:
    DataType type_;
    int64_t int_val_ = 0;
    double float_val_ = 0.0;
    std::string str_val_;
};

// Column definition
struct ColumnDef {
    std::string name;
    DataType type;
    bool nullable = true;
    bool primary_key = false;
    // 默认值作为字符串存储（解析时使用）
    std::string default_value_str;
    bool has_default = false;
    
    ColumnDef() = default;
    ColumnDef(const std::string& n, DataType t, bool null = true, bool pk = false)
        : name(n), type(t), nullable(null), primary_key(pk) {}
};

// Table schema
struct TableSchema {
    std::string table_name;
    std::vector<ColumnDef> columns;
    page_id_t root_page = INVALID_PAGE_ID;
    
    int GetColumnIndex(const std::string& col_name) const {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i].name == col_name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
};

} // namespace minidb
