/**
 * @file expression_evaluator.cpp
 * @brief 表达式求值器实现
 */

#include "expression_evaluator.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <stdexcept>

namespace minidb {

// ============================================================================
// ExpressionEvaluator
// ============================================================================

Value ExpressionEvaluator::Evaluate(const Expression* expr, const Tuple& tuple,
                                     const OutputSchema& schema) {
    if (!expr) {
        return Value::Null();
    }
    
    switch (expr->type) {
        case ExprType::LITERAL:
            return EvaluateLiteral(std::get<LiteralExpr>(expr->data));
            
        case ExprType::COLUMN_REF:
            return EvaluateColumnRef(std::get<ColumnRefExpr>(expr->data), tuple, schema);
            
        case ExprType::BINARY_OP:
            return EvaluateBinaryOp(std::get<BinaryOpExpr>(expr->data), tuple, schema);
            
        case ExprType::UNARY_OP:
            return EvaluateUnaryOp(std::get<UnaryOpExpr>(expr->data), tuple, schema);
            
        case ExprType::LIKE:
            return EvaluateLike(std::get<LikeExpr>(expr->data), tuple, schema);
            
        case ExprType::IS_NULL:
            return EvaluateIsNull(std::get<IsNullExpr>(expr->data), tuple, schema);
            
        case ExprType::IN_LIST:
            return EvaluateIn(std::get<InExpr>(expr->data), tuple, schema);
            
        case ExprType::BETWEEN:
            return EvaluateBetween(std::get<BetweenExpr>(expr->data), tuple, schema);
            
        case ExprType::FUNCTION_CALL: {
            // 对于聚合函数，尝试从 schema 中按名称查找结果
            // 这在 HAVING 子句中很有用，因为聚合结果已经计算完成
            const auto& func = std::get<FunctionCallExpr>(expr->data);
            if (func.IsAggregate()) {
                // 尝试按函数名查找列（如 "COUNT", "AVG", "SUM" 等）
                int idx = schema.GetColumnIndex(func.func_name, "");
                if (idx >= 0 && idx < static_cast<int>(tuple.values.size())) {
                    return tuple.values[idx];
                }
            }
            // 普通函数调用（非聚合）
            // TODO: 实现常见函数如 UPPER, LOWER, LENGTH 等
            return Value::Null();
        }
            
        case ExprType::SUBQUERY:
            // 子查询需要单独执行
            // TODO: 实现子查询求值
            return Value::Null();
            
        case ExprType::CASE_EXPR:
            // TODO: 实现CASE表达式
            return Value::Null();
            
        default:
            return Value::Null();
    }
}

Value ExpressionEvaluator::Evaluate(const Expression* expr,
                                     const Tuple& left_tuple, const OutputSchema& left_schema,
                                     const Tuple& right_tuple, const OutputSchema& right_schema) {
    if (!expr) {
        return Value::Null();
    }
    
    // 对于JOIN，需要从两个元组中查找列
    switch (expr->type) {
        case ExprType::LITERAL:
            return EvaluateLiteral(std::get<LiteralExpr>(expr->data));
            
        case ExprType::COLUMN_REF:
            return EvaluateColumnRefJoin(std::get<ColumnRefExpr>(expr->data),
                                          left_tuple, left_schema,
                                          right_tuple, right_schema);
            
        case ExprType::BINARY_OP: {
            const auto& binop = std::get<BinaryOpExpr>(expr->data);
            Value left_val = Evaluate(binop.left.get(), left_tuple, left_schema,
                                       right_tuple, right_schema);
            Value right_val = Evaluate(binop.right.get(), left_tuple, left_schema,
                                        right_tuple, right_schema);
            return ApplyBinaryOp(binop.op, left_val, right_val);
        }
            
        case ExprType::UNARY_OP: {
            const auto& unop = std::get<UnaryOpExpr>(expr->data);
            Value val = Evaluate(unop.operand.get(), left_tuple, left_schema,
                                  right_tuple, right_schema);
            if (unop.op == UnaryOpType::NOT) {
                return Value(static_cast<int64_t>(!IsTrue(val)));
            } else if (unop.op == UnaryOpType::NEG) {
                if (val.GetType() == DataType::INT) {
                    return Value(-val.GetInt());
                } else if (val.GetType() == DataType::FLOAT) {
                    return Value(-val.GetFloat());
                }
            }
            return Value::Null();
        }
            
        default:
            // 其他类型暂时使用单元组求值
            // 合并元组
            Tuple merged;
            merged.values = left_tuple.values;
            merged.values.insert(merged.values.end(), 
                                  right_tuple.values.begin(), right_tuple.values.end());
            
            OutputSchema merged_schema;
            merged_schema.columns = left_schema.columns;
            merged_schema.columns.insert(merged_schema.columns.end(),
                                          right_schema.columns.begin(), 
                                          right_schema.columns.end());
            
            return Evaluate(expr, merged, merged_schema);
    }
}

bool ExpressionEvaluator::IsTrue(const Value& val) {
    if (val.IsNull()) {
        return false;
    }
    
    switch (val.GetType()) {
        case DataType::INT:
            return val.GetInt() != 0;
        case DataType::FLOAT:
            return val.GetFloat() != 0.0;
        case DataType::TEXT:
            return !val.GetText().empty();
        default:
            return false;
    }
}

Value ExpressionEvaluator::EvaluateLiteral(const LiteralExpr& lit) {
    if (lit.IsNull()) {
        return Value::Null();
    } else if (lit.IsInt()) {
        return Value(lit.GetInt());
    } else if (lit.IsFloat()) {
        return Value(lit.GetFloat());
    } else if (lit.IsString()) {
        return Value(lit.GetString());
    }
    return Value::Null();
}

Value ExpressionEvaluator::EvaluateColumnRef(const ColumnRefExpr& col, 
                                              const Tuple& tuple,
                                              const OutputSchema& schema) {
    int idx = schema.GetColumnIndex(col.column_name, col.table_name);
    if (idx < 0 || idx >= static_cast<int>(tuple.values.size())) {
        // 列不存在
        return Value::Null();
    }
    return tuple.values[idx];
}

Value ExpressionEvaluator::EvaluateColumnRefJoin(const ColumnRefExpr& col,
                                                  const Tuple& left_tuple, 
                                                  const OutputSchema& left_schema,
                                                  const Tuple& right_tuple, 
                                                  const OutputSchema& right_schema) {
    // 先在左表查找
    int idx = left_schema.GetColumnIndex(col.column_name, col.table_name);
    if (idx >= 0 && idx < static_cast<int>(left_tuple.values.size())) {
        return left_tuple.values[idx];
    }
    
    // 再在右表查找
    idx = right_schema.GetColumnIndex(col.column_name, col.table_name);
    if (idx >= 0 && idx < static_cast<int>(right_tuple.values.size())) {
        return right_tuple.values[idx];
    }
    
    return Value::Null();
}

Value ExpressionEvaluator::EvaluateBinaryOp(const BinaryOpExpr& binop, 
                                             const Tuple& tuple,
                                             const OutputSchema& schema) {
    Value left_val = Evaluate(binop.left.get(), tuple, schema);
    Value right_val = Evaluate(binop.right.get(), tuple, schema);
    return ApplyBinaryOp(binop.op, left_val, right_val);
}

Value ExpressionEvaluator::ApplyBinaryOp(BinaryOpType op, const Value& left, 
                                          const Value& right) {
    // NULL处理：大多数操作中NULL参与运算结果为NULL
    // 但AND/OR有特殊处理
    
    switch (op) {
        // 逻辑运算（三值逻辑）
        case BinaryOpType::AND: {
            bool left_true = IsTrue(left);
            bool right_true = IsTrue(right);
            bool left_null = left.IsNull();
            bool right_null = right.IsNull();
            
            if (!left_true && !left_null) return Value(int64_t(0));  // FALSE AND x = FALSE
            if (!right_true && !right_null) return Value(int64_t(0));  // x AND FALSE = FALSE
            if (left_null || right_null) return Value::Null();  // NULL AND TRUE = NULL
            return Value(int64_t(1));  // TRUE AND TRUE = TRUE
        }
        
        case BinaryOpType::OR: {
            bool left_true = IsTrue(left);
            bool right_true = IsTrue(right);
            bool left_null = left.IsNull();
            bool right_null = right.IsNull();
            
            if (left_true) return Value(int64_t(1));  // TRUE OR x = TRUE
            if (right_true) return Value(int64_t(1));  // x OR TRUE = TRUE
            if (left_null || right_null) return Value::Null();  // NULL OR FALSE = NULL
            return Value(int64_t(0));  // FALSE OR FALSE = FALSE
        }
        
        default:
            break;
    }
    
    // 其他运算：NULL参与则结果为NULL
    if (left.IsNull() || right.IsNull()) {
        return Value::Null();
    }
    
    // 算术运算
    switch (op) {
        case BinaryOpType::ADD: {
            if (left.GetType() == DataType::INT && right.GetType() == DataType::INT) {
                return Value(left.GetInt() + right.GetInt());
            }
            // 类型提升为浮点
            double l = (left.GetType() == DataType::INT) ? 
                       static_cast<double>(left.GetInt()) : left.GetFloat();
            double r = (right.GetType() == DataType::INT) ? 
                       static_cast<double>(right.GetInt()) : right.GetFloat();
            return Value(l + r);
        }
        
        case BinaryOpType::SUB: {
            if (left.GetType() == DataType::INT && right.GetType() == DataType::INT) {
                return Value(left.GetInt() - right.GetInt());
            }
            double l = (left.GetType() == DataType::INT) ? 
                       static_cast<double>(left.GetInt()) : left.GetFloat();
            double r = (right.GetType() == DataType::INT) ? 
                       static_cast<double>(right.GetInt()) : right.GetFloat();
            return Value(l - r);
        }
        
        case BinaryOpType::MUL: {
            if (left.GetType() == DataType::INT && right.GetType() == DataType::INT) {
                return Value(left.GetInt() * right.GetInt());
            }
            double l = (left.GetType() == DataType::INT) ? 
                       static_cast<double>(left.GetInt()) : left.GetFloat();
            double r = (right.GetType() == DataType::INT) ? 
                       static_cast<double>(right.GetInt()) : right.GetFloat();
            return Value(l * r);
        }
        
        case BinaryOpType::DIV: {
            double l = (left.GetType() == DataType::INT) ? 
                       static_cast<double>(left.GetInt()) : left.GetFloat();
            double r = (right.GetType() == DataType::INT) ? 
                       static_cast<double>(right.GetInt()) : right.GetFloat();
            if (r == 0.0) return Value::Null();  // 除零
            return Value(l / r);
        }
        
        case BinaryOpType::MOD: {
            if (left.GetType() == DataType::INT && right.GetType() == DataType::INT) {
                if (right.GetInt() == 0) return Value::Null();
                return Value(left.GetInt() % right.GetInt());
            }
            double l = (left.GetType() == DataType::INT) ? 
                       static_cast<double>(left.GetInt()) : left.GetFloat();
            double r = (right.GetType() == DataType::INT) ? 
                       static_cast<double>(right.GetInt()) : right.GetFloat();
            if (r == 0.0) return Value::Null();
            return Value(std::fmod(l, r));
        }
        
        // 比较运算
        case BinaryOpType::EQ:
            return Value(static_cast<int64_t>(left == right ? 1 : 0));
            
        case BinaryOpType::NE:
            return Value(static_cast<int64_t>(left != right ? 1 : 0));
            
        case BinaryOpType::LT:
            return Value(static_cast<int64_t>(left < right ? 1 : 0));
            
        case BinaryOpType::LE:
            return Value(static_cast<int64_t>(left <= right ? 1 : 0));
            
        case BinaryOpType::GT:
            return Value(static_cast<int64_t>(left > right ? 1 : 0));
            
        case BinaryOpType::GE:
            return Value(static_cast<int64_t>(left >= right ? 1 : 0));
            
        default:
            return Value::Null();
    }
}

Value ExpressionEvaluator::EvaluateUnaryOp(const UnaryOpExpr& unop, 
                                            const Tuple& tuple,
                                            const OutputSchema& schema) {
    Value val = Evaluate(unop.operand.get(), tuple, schema);
    
    if (unop.op == UnaryOpType::NOT) {
        if (val.IsNull()) return Value::Null();
        return Value(static_cast<int64_t>(!IsTrue(val)));
    } else if (unop.op == UnaryOpType::NEG) {
        if (val.IsNull()) return Value::Null();
        if (val.GetType() == DataType::INT) {
            return Value(-val.GetInt());
        } else if (val.GetType() == DataType::FLOAT) {
            return Value(-val.GetFloat());
        }
    }
    return Value::Null();
}

Value ExpressionEvaluator::EvaluateLike(const LikeExpr& like, const Tuple& tuple,
                                         const OutputSchema& schema) {
    Value val = Evaluate(like.operand.get(), tuple, schema);
    if (val.IsNull() || val.GetType() != DataType::TEXT) {
        return Value::Null();
    }
    
    bool match = MatchLike(val.GetText(), like.pattern);
    if (like.is_not) {
        match = !match;
    }
    return Value(static_cast<int64_t>(match ? 1 : 0));
}

bool ExpressionEvaluator::MatchLike(const std::string& str, const std::string& pattern) {
    // 简单的LIKE匹配：%匹配任意字符序列，_匹配单个字符
    size_t si = 0, pi = 0;
    size_t star_idx = std::string::npos;
    size_t match_idx = 0;
    
    while (si < str.size()) {
        if (pi < pattern.size() && (pattern[pi] == str[si] || pattern[pi] == '_')) {
            ++si;
            ++pi;
        } else if (pi < pattern.size() && pattern[pi] == '%') {
            star_idx = pi++;
            match_idx = si;
        } else if (star_idx != std::string::npos) {
            pi = star_idx + 1;
            si = ++match_idx;
        } else {
            return false;
        }
    }
    
    while (pi < pattern.size() && pattern[pi] == '%') {
        ++pi;
    }
    
    return pi == pattern.size();
}

Value ExpressionEvaluator::EvaluateIsNull(const IsNullExpr& isnull, 
                                           const Tuple& tuple,
                                           const OutputSchema& schema) {
    Value val = Evaluate(isnull.operand.get(), tuple, schema);
    bool is_null = val.IsNull();
    if (isnull.is_not) {
        is_null = !is_null;
    }
    return Value(static_cast<int64_t>(is_null ? 1 : 0));
}

Value ExpressionEvaluator::EvaluateIn(const InExpr& in, const Tuple& tuple,
                                       const OutputSchema& schema) {
    Value val = Evaluate(in.operand.get(), tuple, schema);
    if (val.IsNull()) {
        return Value::Null();
    }
    
    bool found = false;
    bool has_null = false;
    
    for (const auto& item : in.values) {
        Value item_val = Evaluate(item.get(), tuple, schema);
        if (item_val.IsNull()) {
            has_null = true;
        } else if (val == item_val) {
            found = true;
            break;
        }
    }
    
    if (found) {
        return Value(static_cast<int64_t>(in.is_not ? 0 : 1));
    }
    if (has_null) {
        return Value::Null();
    }
    return Value(static_cast<int64_t>(in.is_not ? 1 : 0));
}

Value ExpressionEvaluator::EvaluateBetween(const BetweenExpr& between, 
                                            const Tuple& tuple,
                                            const OutputSchema& schema) {
    Value val = Evaluate(between.operand.get(), tuple, schema);
    Value low = Evaluate(between.low.get(), tuple, schema);
    Value high = Evaluate(between.high.get(), tuple, schema);
    
    if (val.IsNull() || low.IsNull() || high.IsNull()) {
        return Value::Null();
    }
    
    bool in_range = (val >= low) && (val <= high);
    if (between.is_not) {
        in_range = !in_range;
    }
    return Value(static_cast<int64_t>(in_range ? 1 : 0));
}

// ============================================================================
// ExpressionUtil
// ============================================================================

bool ExpressionUtil::HasAggregate(const Expression* expr) {
    if (!expr) return false;
    
    switch (expr->type) {
        case ExprType::FUNCTION_CALL: {
            const auto& func = std::get<FunctionCallExpr>(expr->data);
            return func.IsAggregate();
        }
        
        case ExprType::BINARY_OP: {
            const auto& binop = std::get<BinaryOpExpr>(expr->data);
            return HasAggregate(binop.left.get()) || HasAggregate(binop.right.get());
        }
        
        case ExprType::UNARY_OP: {
            const auto& unop = std::get<UnaryOpExpr>(expr->data);
            return HasAggregate(unop.operand.get());
        }
        
        default:
            return false;
    }
}

std::vector<ColumnRefExpr> ExpressionUtil::GetColumnRefs(const Expression* expr) {
    std::vector<ColumnRefExpr> refs;
    if (!expr) return refs;
    
    switch (expr->type) {
        case ExprType::COLUMN_REF:
            refs.push_back(std::get<ColumnRefExpr>(expr->data));
            break;
            
        case ExprType::BINARY_OP: {
            const auto& binop = std::get<BinaryOpExpr>(expr->data);
            auto left_refs = GetColumnRefs(binop.left.get());
            auto right_refs = GetColumnRefs(binop.right.get());
            refs.insert(refs.end(), left_refs.begin(), left_refs.end());
            refs.insert(refs.end(), right_refs.begin(), right_refs.end());
            break;
        }
        
        case ExprType::UNARY_OP: {
            const auto& unop = std::get<UnaryOpExpr>(expr->data);
            refs = GetColumnRefs(unop.operand.get());
            break;
        }
        
        case ExprType::FUNCTION_CALL: {
            const auto& func = std::get<FunctionCallExpr>(expr->data);
            for (const auto& arg : func.args) {
                auto arg_refs = GetColumnRefs(arg.get());
                refs.insert(refs.end(), arg_refs.begin(), arg_refs.end());
            }
            break;
        }
        
        default:
            break;
    }
    
    return refs;
}

std::unique_ptr<Expression> ExpressionUtil::Clone(const Expression* expr) {
    if (!expr) return nullptr;
    
    auto result = std::make_unique<Expression>();
    result->type = expr->type;
    
    switch (expr->type) {
        case ExprType::LITERAL: {
            result->data = std::get<LiteralExpr>(expr->data);
            break;
        }
        
        case ExprType::COLUMN_REF: {
            result->data = std::get<ColumnRefExpr>(expr->data);
            break;
        }
        
        case ExprType::BINARY_OP: {
            const auto& binop = std::get<BinaryOpExpr>(expr->data);
            BinaryOpExpr new_binop;
            new_binop.op = binop.op;
            new_binop.left = Clone(binop.left.get());
            new_binop.right = Clone(binop.right.get());
            result->data = std::move(new_binop);
            break;
        }
        
        case ExprType::UNARY_OP: {
            const auto& unop = std::get<UnaryOpExpr>(expr->data);
            UnaryOpExpr new_unop;
            new_unop.op = unop.op;
            new_unop.operand = Clone(unop.operand.get());
            result->data = std::move(new_unop);
            break;
        }
        
        case ExprType::LIKE: {
            const auto& like = std::get<LikeExpr>(expr->data);
            LikeExpr new_like;
            new_like.operand = Clone(like.operand.get());
            new_like.pattern = like.pattern;
            new_like.is_not = like.is_not;
            result->data = std::move(new_like);
            break;
        }
        
        case ExprType::IS_NULL: {
            const auto& isnull = std::get<IsNullExpr>(expr->data);
            IsNullExpr new_isnull;
            new_isnull.operand = Clone(isnull.operand.get());
            new_isnull.is_not = isnull.is_not;
            result->data = std::move(new_isnull);
            break;
        }
        
        case ExprType::IN_LIST: {
            const auto& in = std::get<InExpr>(expr->data);
            InExpr new_in;
            new_in.operand = Clone(in.operand.get());
            new_in.is_not = in.is_not;
            for (const auto& val : in.values) {
                new_in.values.push_back(Clone(val.get()));
            }
            result->data = std::move(new_in);
            break;
        }
        
        case ExprType::BETWEEN: {
            const auto& between = std::get<BetweenExpr>(expr->data);
            BetweenExpr new_between;
            new_between.operand = Clone(between.operand.get());
            new_between.low = Clone(between.low.get());
            new_between.high = Clone(between.high.get());
            new_between.is_not = between.is_not;
            result->data = std::move(new_between);
            break;
        }
        
        case ExprType::FUNCTION_CALL: {
            const auto& func = std::get<FunctionCallExpr>(expr->data);
            FunctionCallExpr new_func;
            new_func.func_name = func.func_name;
            new_func.is_distinct = func.is_distinct;
            for (const auto& arg : func.args) {
                new_func.args.push_back(Clone(arg.get()));
            }
            result->data = std::move(new_func);
            break;
        }
        
        default:
            // 其他类型暂不支持克隆
            break;
    }
    
    return result;
}

} // namespace minidb
