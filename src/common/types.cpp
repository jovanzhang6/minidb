/**
 * @file types.cpp
 * @brief Common type implementations
 */

#include "common/types.h"
#include <sstream>
#include <iomanip>

namespace minidb {

bool Value::operator==(const Value& other) const {
    if (type_ != other.type_) return false;
    if (IsNull() && other.IsNull()) return true;
    
    switch (type_) {
        case DataType::INT:
            return int_val_ == other.int_val_;
        case DataType::FLOAT:
            return float_val_ == other.float_val_;
        case DataType::TEXT:
            return str_val_ == other.str_val_;
        default:
            return false;
    }
}

bool Value::operator<(const Value& other) const {
    if (IsNull() && !other.IsNull()) return true;
    if (!IsNull() && other.IsNull()) return false;
    if (IsNull() && other.IsNull()) return false;
    
    if (type_ != other.type_) {
        return static_cast<int>(type_) < static_cast<int>(other.type_);
    }
    
    switch (type_) {
        case DataType::INT:
            return int_val_ < other.int_val_;
        case DataType::FLOAT:
            return float_val_ < other.float_val_;
        case DataType::TEXT:
            return str_val_ < other.str_val_;
        default:
            return false;
    }
}

bool Value::operator>(const Value& other) const {
    return other < *this;
}

bool Value::operator<=(const Value& other) const {
    return !(other < *this);
}

bool Value::operator>=(const Value& other) const {
    return !(*this < other);
}

bool Value::operator!=(const Value& other) const {
    return !(*this == other);
}

std::string Value::ToString() const {
    if (IsNull()) {
        return "NULL";
    }
    
    std::ostringstream oss;
    switch (type_) {
        case DataType::INT:
            oss << int_val_;
            break;
        case DataType::FLOAT:
            oss << std::fixed << std::setprecision(6) << float_val_;
            break;
        case DataType::TEXT:
            oss << "'" << str_val_ << "'";
            break;
        default:
            oss << "<invalid>";
            break;
    }
    return oss.str();
}

} // namespace minidb
