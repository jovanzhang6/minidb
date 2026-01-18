/**
 * @file ast.cpp
 * @brief AST节点辅助函数实现
 */

#include "parser/ast.h"
#include <algorithm>
#include <cctype>

namespace minidb {

// ============================================================================
// Expression 便捷构造函数
// ============================================================================

std::unique_ptr<Expression> Expression::MakeLiteral(LiteralValue value) {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::LITERAL;
    LiteralExpr lit;
    lit.value = std::move(value);
    expr->data = std::move(lit);
    return expr;
}

std::unique_ptr<Expression> Expression::MakeColumnRef(const std::string& col,
                                                       const std::string& table) {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::COLUMN_REF;
    ColumnRefExpr ref;
    ref.column_name = col;
    ref.table_name = table;
    expr->data = std::move(ref);
    return expr;
}

std::unique_ptr<Expression> Expression::MakeBinaryOp(BinaryOpType op,
                                                      std::unique_ptr<Expression> left,
                                                      std::unique_ptr<Expression> right) {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::BINARY_OP;
    BinaryOpExpr bin;
    bin.op = op;
    bin.left = std::move(left);
    bin.right = std::move(right);
    expr->data = std::move(bin);
    return expr;
}

std::unique_ptr<Expression> Expression::MakeUnaryOp(UnaryOpType op,
                                                     std::unique_ptr<Expression> operand) {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::UNARY_OP;
    UnaryOpExpr unary;
    unary.op = op;
    unary.operand = std::move(operand);
    expr->data = std::move(unary);
    return expr;
}

// ============================================================================
// FunctionCallExpr 辅助函数
// ============================================================================

bool FunctionCallExpr::IsAggregate() const {
    std::string upper_name = func_name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);
    return upper_name == "COUNT" || upper_name == "SUM" || 
           upper_name == "AVG" || upper_name == "MIN" || upper_name == "MAX";
}

AggFuncType FunctionCallExpr::GetAggType() const {
    std::string upper_name = func_name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);
    
    if (upper_name == "COUNT") {
        // 检查是否是 COUNT(*)
        if (args.empty()) {
            return AggFuncType::COUNT_STAR;
        }
        return AggFuncType::COUNT;
    }
    if (upper_name == "SUM") return AggFuncType::SUM;
    if (upper_name == "AVG") return AggFuncType::AVG;
    if (upper_name == "MIN") return AggFuncType::MIN;
    if (upper_name == "MAX") return AggFuncType::MAX;
    
    // 默认返回COUNT
    return AggFuncType::COUNT;
}

// ============================================================================
// 类型转换辅助函数
// ============================================================================

std::string DataTypeToString(DataType type) {
    switch (type) {
        case DataType::INT:   return "INT";
        case DataType::FLOAT: return "FLOAT";
        case DataType::TEXT:  return "TEXT";
        default:              return "UNKNOWN";
    }
}

DataType StringToDataType(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    if (upper == "INT" || upper == "INTEGER") return DataType::INT;
    if (upper == "FLOAT" || upper == "REAL" || upper == "DOUBLE") return DataType::FLOAT;
    if (upper == "TEXT" || upper == "VARCHAR" || upper == "STRING") return DataType::TEXT;
    
    // 默认返回TEXT
    return DataType::TEXT;
}

std::string BinaryOpToString(BinaryOpType op) {
    switch (op) {
        case BinaryOpType::ADD: return "+";
        case BinaryOpType::SUB: return "-";
        case BinaryOpType::MUL: return "*";
        case BinaryOpType::DIV: return "/";
        case BinaryOpType::MOD: return "%";
        case BinaryOpType::EQ:  return "=";
        case BinaryOpType::NE:  return "<>";
        case BinaryOpType::LT:  return "<";
        case BinaryOpType::LE:  return "<=";
        case BinaryOpType::GT:  return ">";
        case BinaryOpType::GE:  return ">=";
        case BinaryOpType::AND: return "AND";
        case BinaryOpType::OR:  return "OR";
        default:                return "?";
    }
}

std::string StmtTypeToString(StmtType type) {
    switch (type) {
        case StmtType::CREATE_TABLE: return "CREATE_TABLE";
        case StmtType::DROP_TABLE:   return "DROP_TABLE";
        case StmtType::ALTER_TABLE:  return "ALTER_TABLE";
        case StmtType::CREATE_INDEX: return "CREATE_INDEX";
        case StmtType::DROP_INDEX:   return "DROP_INDEX";
        case StmtType::INSERT:       return "INSERT";
        case StmtType::UPDATE:       return "UPDATE";
        case StmtType::DELETE_STMT:  return "DELETE";
        case StmtType::SELECT:       return "SELECT";
        case StmtType::CREATE_USER:  return "CREATE_USER";
        case StmtType::DROP_USER:    return "DROP_USER";
        case StmtType::GRANT:        return "GRANT";
        case StmtType::REVOKE:       return "REVOKE";
        case StmtType::BEGIN_TXN:    return "BEGIN";
        case StmtType::COMMIT:       return "COMMIT";
        case StmtType::ROLLBACK:     return "ROLLBACK";
        default:                     return "UNKNOWN";
    }
}

}  // namespace minidb
