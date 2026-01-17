/**
 * @file parser.cpp
 * @brief SQL解析器封装类实现
 */

#include "parser/parser.h"
#include "parser/ast.h"
#include <iostream>
#include <iomanip>

// Flex/Bison生成的头文件
#include "parser.tab.hpp"

// 前向声明
struct yy_buffer_state;
typedef struct yy_buffer_state* YY_BUFFER_STATE;

// Flex函数声明（在lexer.l中定义）
extern YY_BUFFER_STATE yy_scan_string(const char* str, void* scanner);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer, void* scanner);
extern int yylex_init(void** scanner);
extern int yylex_destroy(void* scanner);

// Bison函数声明
extern int yyparse(void* scanner, minidb::Statement** result, const char** error_msg);

namespace minidb {

std::unique_ptr<Statement> Parser::Parse(const std::string& sql) {
    error_message_.clear();
    
    // 初始化scanner
    void* scanner = nullptr;
    if (yylex_init(&scanner) != 0) {
        error_message_ = "Failed to initialize lexer";
        return nullptr;
    }
    
    // 设置输入字符串
    YY_BUFFER_STATE buffer = yy_scan_string(sql.c_str(), scanner);
    
    // 解析
    Statement* result = nullptr;
    const char* error_msg = nullptr;
    int parse_result = yyparse(scanner, &result, &error_msg);
    
    // 清理
    yy_delete_buffer(buffer, scanner);
    yylex_destroy(scanner);
    
    // 检查结果
    if (parse_result != 0 || result == nullptr) {
        if (error_msg) {
            error_message_ = error_msg;
        } else {
            error_message_ = "Unknown parse error";
        }
        delete result;
        return nullptr;
    }
    
    return std::unique_ptr<Statement>(result);
}

// ============================================================================
// 调试打印函数
// ============================================================================

static void PrintIndent(int indent) {
    for (int i = 0; i < indent; i++) {
        std::cout << "  ";
    }
}

void PrintExpression(const Expression* expr, int indent) {
    if (!expr) {
        PrintIndent(indent);
        std::cout << "(null)" << std::endl;
        return;
    }
    
    PrintIndent(indent);
    
    switch (expr->type) {
        case ExprType::LITERAL: {
            const auto& lit = std::get<LiteralExpr>(expr->data);
            if (lit.IsNull()) {
                std::cout << "NULL" << std::endl;
            } else if (lit.IsInt()) {
                std::cout << "INT: " << lit.GetInt() << std::endl;
            } else if (lit.IsFloat()) {
                std::cout << "FLOAT: " << lit.GetFloat() << std::endl;
            } else if (lit.IsString()) {
                std::cout << "STRING: '" << lit.GetString() << "'" << std::endl;
            }
            break;
        }
        
        case ExprType::COLUMN_REF: {
            const auto& ref = std::get<ColumnRefExpr>(expr->data);
            std::cout << "COLUMN: ";
            if (!ref.table_name.empty()) {
                std::cout << ref.table_name << ".";
            }
            std::cout << ref.column_name << std::endl;
            break;
        }
        
        case ExprType::BINARY_OP: {
            const auto& bin = std::get<BinaryOpExpr>(expr->data);
            std::cout << "BINARY_OP: " << BinaryOpToString(bin.op) << std::endl;
            PrintExpression(bin.left.get(), indent + 1);
            PrintExpression(bin.right.get(), indent + 1);
            break;
        }
        
        case ExprType::UNARY_OP: {
            const auto& unary = std::get<UnaryOpExpr>(expr->data);
            std::cout << "UNARY_OP: " << (unary.op == UnaryOpType::NEG ? "-" : "NOT") << std::endl;
            PrintExpression(unary.operand.get(), indent + 1);
            break;
        }
        
        case ExprType::FUNCTION_CALL: {
            const auto& func = std::get<FunctionCallExpr>(expr->data);
            std::cout << "FUNCTION: " << func.func_name;
            if (func.is_distinct) std::cout << " DISTINCT";
            std::cout << std::endl;
            for (const auto& arg : func.args) {
                PrintExpression(arg.get(), indent + 1);
            }
            break;
        }
        
        case ExprType::LIKE: {
            const auto& like = std::get<LikeExpr>(expr->data);
            std::cout << (like.is_not ? "NOT LIKE" : "LIKE") << ": '" << like.pattern << "'" << std::endl;
            PrintExpression(like.operand.get(), indent + 1);
            break;
        }
        
        case ExprType::IS_NULL: {
            const auto& is_null = std::get<IsNullExpr>(expr->data);
            std::cout << (is_null.is_not ? "IS NOT NULL" : "IS NULL") << std::endl;
            PrintExpression(is_null.operand.get(), indent + 1);
            break;
        }
        
        case ExprType::IN_LIST: {
            const auto& in_expr = std::get<InExpr>(expr->data);
            std::cout << (in_expr.is_not ? "NOT IN" : "IN") << std::endl;
            PrintIndent(indent + 1);
            std::cout << "operand:" << std::endl;
            PrintExpression(in_expr.operand.get(), indent + 2);
            PrintIndent(indent + 1);
            std::cout << "values:" << std::endl;
            for (const auto& val : in_expr.values) {
                PrintExpression(val.get(), indent + 2);
            }
            break;
        }
        
        case ExprType::BETWEEN: {
            const auto& between = std::get<BetweenExpr>(expr->data);
            std::cout << (between.is_not ? "NOT BETWEEN" : "BETWEEN") << std::endl;
            PrintIndent(indent + 1);
            std::cout << "operand:" << std::endl;
            PrintExpression(between.operand.get(), indent + 2);
            PrintIndent(indent + 1);
            std::cout << "low:" << std::endl;
            PrintExpression(between.low.get(), indent + 2);
            PrintIndent(indent + 1);
            std::cout << "high:" << std::endl;
            PrintExpression(between.high.get(), indent + 2);
            break;
        }
        
        default:
            std::cout << "UNKNOWN_EXPR" << std::endl;
            break;
    }
}

void PrintAST(const Statement* stmt, int indent) {
    if (!stmt) {
        PrintIndent(indent);
        std::cout << "(null statement)" << std::endl;
        return;
    }
    
    PrintIndent(indent);
    std::cout << "Statement: " << StmtTypeToString(stmt->type) << std::endl;
    
    switch (stmt->type) {
        case StmtType::CREATE_TABLE: {
            const auto& create = std::get<CreateTableStmt>(stmt->data);
            PrintIndent(indent + 1);
            std::cout << "Table: " << create.table_name << std::endl;
            PrintIndent(indent + 1);
            std::cout << "Columns:" << std::endl;
            for (const auto& col : create.columns) {
                PrintIndent(indent + 2);
                std::cout << col.name << " " << DataTypeToString(col.type);
                if (!col.nullable) std::cout << " NOT NULL";
                if (col.primary_key) std::cout << " PRIMARY KEY";
                std::cout << std::endl;
            }
            break;
        }
        
        case StmtType::DROP_TABLE: {
            const auto& drop = std::get<DropTableStmt>(stmt->data);
            PrintIndent(indent + 1);
            std::cout << "Table: " << drop.table_name << std::endl;
            break;
        }
        
        case StmtType::INSERT: {
            const auto& insert = std::get<InsertStmt>(stmt->data);
            PrintIndent(indent + 1);
            std::cout << "Table: " << insert.table_name << std::endl;
            if (!insert.column_names.empty()) {
                PrintIndent(indent + 1);
                std::cout << "Columns: ";
                for (size_t i = 0; i < insert.column_names.size(); i++) {
                    if (i > 0) std::cout << ", ";
                    std::cout << insert.column_names[i];
                }
                std::cout << std::endl;
            }
            PrintIndent(indent + 1);
            std::cout << "Values: " << insert.values.size() << " row(s)" << std::endl;
            for (size_t i = 0; i < insert.values.size(); i++) {
                PrintIndent(indent + 2);
                std::cout << "Row " << i << ":" << std::endl;
                for (const auto& val : insert.values[i]) {
                    PrintExpression(val.get(), indent + 3);
                }
            }
            break;
        }
        
        case StmtType::UPDATE: {
            const auto& update = std::get<UpdateStmt>(stmt->data);
            PrintIndent(indent + 1);
            std::cout << "Table: " << update.table_name << std::endl;
            PrintIndent(indent + 1);
            std::cout << "Set:" << std::endl;
            for (const auto& item : update.updates) {
                PrintIndent(indent + 2);
                std::cout << item.column_name << " = " << std::endl;
                PrintExpression(item.value.get(), indent + 3);
            }
            if (update.where_clause) {
                PrintIndent(indent + 1);
                std::cout << "Where:" << std::endl;
                PrintExpression(update.where_clause.get(), indent + 2);
            }
            break;
        }
        
        case StmtType::DELETE_STMT: {
            const auto& del = std::get<DeleteStmt>(stmt->data);
            PrintIndent(indent + 1);
            std::cout << "Table: " << del.table_name << std::endl;
            if (del.where_clause) {
                PrintIndent(indent + 1);
                std::cout << "Where:" << std::endl;
                PrintExpression(del.where_clause.get(), indent + 2);
            }
            break;
        }
        
        case StmtType::SELECT: {
            const auto& select = std::get<SelectStmt>(stmt->data);
            if (select.is_distinct) {
                PrintIndent(indent + 1);
                std::cout << "DISTINCT" << std::endl;
            }
            PrintIndent(indent + 1);
            std::cout << "Select list:" << std::endl;
            for (const auto& item : select.select_list) {
                if (item.is_star) {
                    PrintIndent(indent + 2);
                    if (!item.star_table.empty()) {
                        std::cout << item.star_table << ".";
                    }
                    std::cout << "*" << std::endl;
                } else {
                    PrintExpression(item.expr.get(), indent + 2);
                    if (!item.alias.empty()) {
                        PrintIndent(indent + 3);
                        std::cout << "AS " << item.alias << std::endl;
                    }
                }
            }
            PrintIndent(indent + 1);
            std::cout << "From:" << std::endl;
            for (const auto& table : select.from_tables) {
                PrintIndent(indent + 2);
                std::cout << table.table_name;
                if (!table.alias.empty()) {
                    std::cout << " AS " << table.alias;
                }
                std::cout << std::endl;
            }
            if (select.where_clause) {
                PrintIndent(indent + 1);
                std::cout << "Where:" << std::endl;
                PrintExpression(select.where_clause.get(), indent + 2);
            }
            if (!select.order_by.empty()) {
                PrintIndent(indent + 1);
                std::cout << "Order by:" << std::endl;
                for (const auto& item : select.order_by) {
                    PrintExpression(item.expr.get(), indent + 2);
                    PrintIndent(indent + 3);
                    std::cout << (item.is_desc ? "DESC" : "ASC") << std::endl;
                }
            }
            if (select.limit >= 0) {
                PrintIndent(indent + 1);
                std::cout << "Limit: " << select.limit << std::endl;
            }
            if (select.offset > 0) {
                PrintIndent(indent + 1);
                std::cout << "Offset: " << select.offset << std::endl;
            }
            break;
        }
        
        case StmtType::CREATE_USER: {
            const auto& user = std::get<CreateUserStmt>(stmt->data);
            PrintIndent(indent + 1);
            std::cout << "Username: " << user.username << std::endl;
            break;
        }
        
        case StmtType::DROP_USER: {
            const auto& user = std::get<DropUserStmt>(stmt->data);
            PrintIndent(indent + 1);
            std::cout << "Username: " << user.username << std::endl;
            break;
        }
        
        case StmtType::GRANT: {
            const auto& grant = std::get<GrantStmt>(stmt->data);
            PrintIndent(indent + 1);
            std::cout << "Privileges: ";
            for (size_t i = 0; i < grant.privileges.size(); i++) {
                if (i > 0) std::cout << ", ";
                switch (grant.privileges[i]) {
                    case AstPrivilegeType::SELECT: std::cout << "SELECT"; break;
                    case AstPrivilegeType::INSERT: std::cout << "INSERT"; break;
                    case AstPrivilegeType::UPDATE: std::cout << "UPDATE"; break;
                    case AstPrivilegeType::DELETE_PRIV: std::cout << "DELETE"; break;
                    case AstPrivilegeType::ALL: std::cout << "ALL"; break;
                }
            }
            std::cout << std::endl;
            PrintIndent(indent + 1);
            std::cout << "Table: " << grant.table_name << std::endl;
            PrintIndent(indent + 1);
            std::cout << "User: " << grant.username << std::endl;
            break;
        }
        
        case StmtType::REVOKE: {
            const auto& revoke = std::get<RevokeStmt>(stmt->data);
            PrintIndent(indent + 1);
            std::cout << "Privileges: ";
            for (size_t i = 0; i < revoke.privileges.size(); i++) {
                if (i > 0) std::cout << ", ";
                switch (revoke.privileges[i]) {
                    case AstPrivilegeType::SELECT: std::cout << "SELECT"; break;
                    case AstPrivilegeType::INSERT: std::cout << "INSERT"; break;
                    case AstPrivilegeType::UPDATE: std::cout << "UPDATE"; break;
                    case AstPrivilegeType::DELETE_PRIV: std::cout << "DELETE"; break;
                    case AstPrivilegeType::ALL: std::cout << "ALL"; break;
                }
            }
            std::cout << std::endl;
            PrintIndent(indent + 1);
            std::cout << "Table: " << revoke.table_name << std::endl;
            PrintIndent(indent + 1);
            std::cout << "User: " << revoke.username << std::endl;
            break;
        }
        
        case StmtType::BEGIN_TXN:
            PrintIndent(indent + 1);
            std::cout << "Begin transaction" << std::endl;
            break;
            
        case StmtType::COMMIT:
            PrintIndent(indent + 1);
            std::cout << "Commit transaction" << std::endl;
            break;
            
        case StmtType::ROLLBACK:
            PrintIndent(indent + 1);
            std::cout << "Rollback transaction" << std::endl;
            break;
            
        default:
            PrintIndent(indent + 1);
            std::cout << "Unknown statement type" << std::endl;
            break;
    }
}

}  // namespace minidb
