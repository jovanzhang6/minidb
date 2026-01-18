/**
 * @file ast.h
 * @brief SQL抽象语法树（AST）节点定义
 * 
 * 定义所有SQL语句的AST节点类型，包括：
 * - DDL: CREATE TABLE, DROP TABLE, ALTER TABLE, CREATE INDEX
 * - DML: INSERT, UPDATE, DELETE
 * - DCL: CREATE USER, DROP USER, GRANT, REVOKE
 * - DQL: SELECT
 * - TCL: BEGIN, COMMIT, ROLLBACK
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include "common/types.h"

namespace minidb {

// 前向声明
struct Expression;
struct Statement;

// ============================================================================
// 表达式类型
// ============================================================================

/**
 * @brief 表达式类型枚举
 */
enum class ExprType {
    LITERAL,        // 字面量：整数、浮点数、字符串、NULL
    COLUMN_REF,     // 列引用：col 或 table.col
    BINARY_OP,      // 二元运算：+, -, *, /, =, <>, <, >, <=, >=, AND, OR
    UNARY_OP,       // 一元运算：NOT, -
    FUNCTION_CALL,  // 函数调用：COUNT(*), SUM(col), AVG(col)
    SUBQUERY,       // 子查询：(SELECT ...)
    IN_LIST,        // IN列表：col IN (1, 2, 3)
    BETWEEN,        // BETWEEN: col BETWEEN a AND b
    LIKE,           // LIKE: col LIKE 'pattern%'
    IS_NULL,        // IS NULL / IS NOT NULL
    CASE_EXPR,      // CASE表达式
};

/**
 * @brief 字面量值类型
 */
using LiteralValue = std::variant<std::monostate, int64_t, double, std::string>;

/**
 * @brief 字面量表达式
 */
struct LiteralExpr {
    LiteralValue value;
    
    bool IsNull() const { return std::holds_alternative<std::monostate>(value); }
    bool IsInt() const { return std::holds_alternative<int64_t>(value); }
    bool IsFloat() const { return std::holds_alternative<double>(value); }
    bool IsString() const { return std::holds_alternative<std::string>(value); }
    
    int64_t GetInt() const { return std::get<int64_t>(value); }
    double GetFloat() const { return std::get<double>(value); }
    const std::string& GetString() const { return std::get<std::string>(value); }
};

/**
 * @brief 列引用表达式
 */
struct ColumnRefExpr {
    std::string table_name;   // 表名（可选）
    std::string column_name;  // 列名
};

/**
 * @brief 二元运算符类型
 */
enum class BinaryOpType {
    // 算术运算
    ADD,        // +
    SUB,        // -
    MUL,        // *
    DIV,        // /
    MOD,        // %
    
    // 比较运算
    EQ,         // =
    NE,         // <> 或 !=
    LT,         // <
    LE,         // <=
    GT,         // >
    GE,         // >=
    
    // 逻辑运算
    AND,        // AND
    OR,         // OR
};

/**
 * @brief 二元运算表达式
 */
struct BinaryOpExpr {
    BinaryOpType op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

/**
 * @brief 一元运算符类型
 */
enum class UnaryOpType {
    NEG,        // -（负号）
    NOT,        // NOT
};

/**
 * @brief 一元运算表达式
 */
struct UnaryOpExpr {
    UnaryOpType op;
    std::unique_ptr<Expression> operand;
};

/**
 * @brief 聚合函数类型
 */
enum class AggFuncType {
    COUNT,
    COUNT_STAR,  // COUNT(*)
    SUM,
    AVG,
    MIN,
    MAX,
};

/**
 * @brief 函数调用表达式
 */
struct FunctionCallExpr {
    std::string func_name;
    std::vector<std::unique_ptr<Expression>> args;
    bool is_distinct = false;  // DISTINCT修饰符
    
    // 聚合函数快捷判断
    bool IsAggregate() const;
    AggFuncType GetAggType() const;
};

/**
 * @brief LIKE表达式
 */
struct LikeExpr {
    std::unique_ptr<Expression> operand;
    std::string pattern;
    bool is_not = false;  // NOT LIKE
};

/**
 * @brief IS NULL表达式
 */
struct IsNullExpr {
    std::unique_ptr<Expression> operand;
    bool is_not = false;  // IS NOT NULL
};

/**
 * @brief IN表达式
 */
struct InExpr {
    std::unique_ptr<Expression> operand;
    std::vector<std::unique_ptr<Expression>> values;  // IN (val1, val2, ...)
    std::unique_ptr<Statement> subquery;              // IN (SELECT ...)
    bool is_not = false;  // NOT IN
};

/**
 * @brief BETWEEN表达式
 */
struct BetweenExpr {
    std::unique_ptr<Expression> operand;
    std::unique_ptr<Expression> low;
    std::unique_ptr<Expression> high;
    bool is_not = false;  // NOT BETWEEN
};

/**
 * @brief 表达式节点（使用variant实现多态）
 */
struct Expression {
    ExprType type;
    std::variant<
        LiteralExpr,
        ColumnRefExpr,
        BinaryOpExpr,
        UnaryOpExpr,
        FunctionCallExpr,
        LikeExpr,
        IsNullExpr,
        InExpr,
        BetweenExpr,
        std::unique_ptr<Statement>  // 子查询
    > data;
    
    // 便捷构造函数
    static std::unique_ptr<Expression> MakeLiteral(LiteralValue value);
    static std::unique_ptr<Expression> MakeColumnRef(const std::string& col, 
                                                      const std::string& table = "");
    static std::unique_ptr<Expression> MakeBinaryOp(BinaryOpType op,
                                                     std::unique_ptr<Expression> left,
                                                     std::unique_ptr<Expression> right);
    static std::unique_ptr<Expression> MakeUnaryOp(UnaryOpType op,
                                                    std::unique_ptr<Expression> operand);
};

// ============================================================================
// 语句类型
// ============================================================================

/**
 * @brief 语句类型枚举
 */
enum class StmtType {
    // DDL
    CREATE_TABLE,
    DROP_TABLE,
    ALTER_TABLE,
    CREATE_INDEX,
    DROP_INDEX,
    
    // DML
    INSERT,
    UPDATE,
    DELETE_STMT,
    
    // DQL
    SELECT,
    
    // DCL
    CREATE_USER,
    DROP_USER,
    GRANT,
    REVOKE,
    
    // TCL
    BEGIN_TXN,
    COMMIT,
    ROLLBACK,
};

// ============================================================================
// DDL语句
// ============================================================================

// 注意：ColumnDef 在 common/types.h 中定义

/**
 * @brief CREATE TABLE语句
 */
struct CreateTableStmt {
    std::string table_name;
    std::vector<ColumnDef> columns;
    bool if_not_exists = false;
};

/**
 * @brief DROP TABLE语句
 */
struct DropTableStmt {
    std::string table_name;
    bool if_exists = false;
};

/**
 * @brief ALTER TABLE操作类型
 */
enum class AlterType {
    ADD_COLUMN,
    DROP_COLUMN,
    RENAME_COLUMN,
    ALTER_COLUMN_TYPE,
    RENAME_TABLE,
};

/**
 * @brief ALTER TABLE语句
 */
struct AlterTableStmt {
    std::string table_name;
    AlterType alter_type;
    ColumnDef column_def;           // ADD COLUMN 使用
    std::string old_column_name;    // RENAME/DROP/ALTER 使用
    std::string new_column_name;    // RENAME COLUMN 使用
    std::string new_table_name;     // RENAME TABLE 使用
    DataType new_type;              // ALTER COLUMN TYPE 使用
};

/**
 * @brief CREATE INDEX语句
 */
struct CreateIndexStmt {
    std::string index_name;
    std::string table_name;
    std::vector<std::string> column_names;  // 为了兼容，保留vector，但只支持单列
    std::string column_name;                 // 单列索引使用（推荐）
    bool is_unique = false;
    bool if_not_exists = false;
};

/**
 * @brief DROP INDEX语句
 */
struct DropIndexStmt {
    std::string index_name;
    bool if_exists = false;
};

// ============================================================================
// DML语句
// ============================================================================

/**
 * @brief INSERT语句
 */
struct InsertStmt {
    std::string table_name;
    std::vector<std::string> column_names;  // 可选：指定列名
    std::vector<std::vector<std::unique_ptr<Expression>>> values;  // 多行值
};

/**
 * @brief UPDATE语句中的SET子句项
 */
struct UpdateItem {
    std::string column_name;
    std::unique_ptr<Expression> value;
};

/**
 * @brief UPDATE语句
 */
struct UpdateStmt {
    std::string table_name;
    std::vector<UpdateItem> updates;
    std::unique_ptr<Expression> where_clause;  // WHERE条件
};

/**
 * @brief DELETE语句
 */
struct DeleteStmt {
    std::string table_name;
    std::unique_ptr<Expression> where_clause;  // WHERE条件
};

// ============================================================================
// DQL语句（SELECT）
// ============================================================================

/**
 * @brief SELECT列表项
 */
struct SelectItem {
    std::unique_ptr<Expression> expr;
    std::string alias;  // AS别名
    bool is_star = false;  // SELECT *
    std::string star_table;  // SELECT table.*
};

/**
 * @brief 表引用（FROM子句）
 */
struct TableRef {
    std::string table_name;
    std::string alias;
    std::unique_ptr<Statement> subquery;  // 子查询作为表
};

/**
 * @brief JOIN类型
 */
enum class JoinType {
    INNER,
    LEFT,
    RIGHT,
    FULL,
    CROSS,
};

/**
 * @brief JOIN子句
 */
struct JoinClause {
    JoinType type;
    TableRef right_table;
    std::unique_ptr<Expression> condition;  // ON条件
};

/**
 * @brief ORDER BY项
 */
struct OrderByItem {
    std::unique_ptr<Expression> expr;
    bool is_desc = false;  // DESC排序
};

/**
 * @brief SELECT语句
 */
struct SelectStmt {
    bool is_distinct = false;
    std::vector<SelectItem> select_list;
    std::vector<TableRef> from_tables;
    std::vector<JoinClause> joins;
    std::unique_ptr<Expression> where_clause;
    std::vector<std::unique_ptr<Expression>> group_by;
    std::unique_ptr<Expression> having_clause;
    std::vector<OrderByItem> order_by;
    int64_t limit = -1;   // -1表示无限制
    int64_t offset = 0;
};

// ============================================================================
// DCL语句
// ============================================================================

/**
 * @brief CREATE USER语句
 */
struct CreateUserStmt {
    std::string username;
    std::string password;
    bool is_admin = false;
};

/**
 * @brief DROP USER语句
 */
struct DropUserStmt {
    std::string username;
};

/**
 * @brief 权限类型（用于AST解析）
 * 
 * 注意：与 catalog.h 中的 PrivilegeType 区分，
 * AST版本用于解析阶段
 */
enum class AstPrivilegeType {
    SELECT,
    INSERT,
    UPDATE,
    DELETE_PRIV,
    ALL,
};

/**
 * @brief GRANT语句
 */
struct GrantStmt {
    std::vector<AstPrivilegeType> privileges;
    std::string table_name;  // ON table
    std::string username;    // TO user
};

/**
 * @brief REVOKE语句
 */
struct RevokeStmt {
    std::vector<AstPrivilegeType> privileges;
    std::string table_name;  // ON table
    std::string username;    // FROM user
};

// ============================================================================
// TCL语句
// ============================================================================

/**
 * @brief BEGIN语句
 */
struct BeginStmt {
    // 暂无额外属性
};

/**
 * @brief COMMIT语句
 */
struct CommitStmt {
    // 暂无额外属性
};

/**
 * @brief ROLLBACK语句
 */
struct RollbackStmt {
    // 暂无额外属性
};

// ============================================================================
// 语句节点
// ============================================================================

/**
 * @brief 语句节点（使用variant实现多态）
 */
struct Statement {
    StmtType type;
    std::variant<
        CreateTableStmt,
        DropTableStmt,
        AlterTableStmt,
        CreateIndexStmt,
        DropIndexStmt,
        InsertStmt,
        UpdateStmt,
        DeleteStmt,
        SelectStmt,
        CreateUserStmt,
        DropUserStmt,
        GrantStmt,
        RevokeStmt,
        BeginStmt,
        CommitStmt,
        RollbackStmt
    > data;
    
    // 便捷获取函数
    template<typename T>
    T& Get() { return std::get<T>(data); }
    
    template<typename T>
    const T& Get() const { return std::get<T>(data); }
};

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * @brief 将DataType转换为字符串
 */
std::string DataTypeToString(DataType type);

/**
 * @brief 从字符串解析DataType
 */
DataType StringToDataType(const std::string& str);

/**
 * @brief 将BinaryOpType转换为字符串
 */
std::string BinaryOpToString(BinaryOpType op);

/**
 * @brief 将StmtType转换为字符串
 */
std::string StmtTypeToString(StmtType type);

}  // namespace minidb
