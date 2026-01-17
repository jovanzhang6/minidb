/**
 * @file parser_test.cpp
 * @brief SQL解析器单元测试（不依赖gtest）
 */

#include <iostream>
#include <cassert>
#include <string>
#include "parser/parser.h"
#include "parser/ast.h"

using namespace minidb;

// 测试统计
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void name()
#define RUN_TEST(name) do { \
    std::cout << "  " << #name << "... "; \
    try { \
        name(); \
        std::cout << "PASSED" << std::endl; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << std::endl; \
        tests_failed++; \
    } catch (...) { \
        std::cout << "FAILED: Unknown exception" << std::endl; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("Assertion failed: " #cond); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " #a " == " #b); \
    } \
} while(0)

// 辅助函数
std::unique_ptr<Statement> ParseSQL(Parser& parser, const std::string& sql) {
    auto stmt = parser.Parse(sql);
    if (!stmt) {
        throw std::runtime_error("Parse failed: " + parser.GetErrorMessage());
    }
    return stmt;
}

// ============================================================================
// DDL 测试
// ============================================================================

TEST(TestCreateTableSimple) {
    Parser parser;
    auto stmt = ParseSQL(parser, "CREATE TABLE users (id INT, name TEXT, score FLOAT)");
    
    ASSERT_EQ(stmt->type, StmtType::CREATE_TABLE);
    const auto& create = std::get<CreateTableStmt>(stmt->data);
    ASSERT_EQ(create.table_name, "users");
    ASSERT_EQ(create.columns.size(), 3u);
    
    ASSERT_EQ(create.columns[0].name, "id");
    ASSERT_EQ(create.columns[0].type, DataType::INT);
    ASSERT_EQ(create.columns[1].name, "name");
    ASSERT_EQ(create.columns[1].type, DataType::TEXT);
    ASSERT_EQ(create.columns[2].name, "score");
    ASSERT_EQ(create.columns[2].type, DataType::FLOAT);
}

TEST(TestCreateTableIfNotExists) {
    Parser parser;
    auto stmt = ParseSQL(parser, "CREATE TABLE IF NOT EXISTS products (id INT NOT NULL, name TEXT)");
    
    const auto& create = std::get<CreateTableStmt>(stmt->data);
    ASSERT(create.if_not_exists);
    ASSERT_EQ(create.table_name, "products");
    ASSERT(!create.columns[0].nullable);  // NOT NULL
    ASSERT(create.columns[1].nullable);    // 默认可为NULL
}

TEST(TestDropTable) {
    Parser parser;
    auto stmt = ParseSQL(parser, "DROP TABLE users");
    
    ASSERT_EQ(stmt->type, StmtType::DROP_TABLE);
    const auto& drop = std::get<DropTableStmt>(stmt->data);
    ASSERT_EQ(drop.table_name, "users");
    ASSERT(!drop.if_exists);
}

TEST(TestDropTableIfExists) {
    Parser parser;
    auto stmt = ParseSQL(parser, "DROP TABLE IF EXISTS temp_data");
    
    const auto& drop = std::get<DropTableStmt>(stmt->data);
    ASSERT(drop.if_exists);
}

TEST(TestAlterTableAddColumn) {
    Parser parser;
    auto stmt = ParseSQL(parser, "ALTER TABLE users ADD COLUMN email TEXT");
    
    ASSERT_EQ(stmt->type, StmtType::ALTER_TABLE);
    const auto& alter = std::get<AlterTableStmt>(stmt->data);
    ASSERT_EQ(alter.table_name, "users");
    ASSERT_EQ(alter.alter_type, AlterType::ADD_COLUMN);
    ASSERT_EQ(alter.column_def.name, "email");
}

TEST(TestCreateIndex) {
    Parser parser;
    auto stmt = ParseSQL(parser, "CREATE INDEX idx_name ON users (name)");
    
    ASSERT_EQ(stmt->type, StmtType::CREATE_INDEX);
    const auto& idx = std::get<CreateIndexStmt>(stmt->data);
    ASSERT_EQ(idx.index_name, "idx_name");
    ASSERT_EQ(idx.table_name, "users");
    ASSERT_EQ(idx.column_names.size(), 1u);
    ASSERT_EQ(idx.column_names[0], "name");
}

// ============================================================================
// DML 测试
// ============================================================================

TEST(TestInsertSimple) {
    Parser parser;
    auto stmt = ParseSQL(parser, "INSERT INTO users VALUES (1, 'Alice', 85.5)");
    
    ASSERT_EQ(stmt->type, StmtType::INSERT);
    const auto& insert = std::get<InsertStmt>(stmt->data);
    ASSERT_EQ(insert.table_name, "users");
    ASSERT(insert.column_names.empty());
    ASSERT_EQ(insert.values.size(), 1u);
    ASSERT_EQ(insert.values[0].size(), 3u);
    
    // 检查第一个值
    const auto& val0 = insert.values[0][0];
    ASSERT_EQ(val0->type, ExprType::LITERAL);
    const auto& lit0 = std::get<LiteralExpr>(val0->data);
    ASSERT(lit0.IsInt());
    ASSERT_EQ(lit0.GetInt(), 1);
}

TEST(TestInsertWithColumns) {
    Parser parser;
    auto stmt = ParseSQL(parser, "INSERT INTO users (name, age) VALUES ('Bob', 25)");
    
    const auto& insert = std::get<InsertStmt>(stmt->data);
    ASSERT_EQ(insert.column_names.size(), 2u);
    ASSERT_EQ(insert.column_names[0], "name");
    ASSERT_EQ(insert.column_names[1], "age");
}

TEST(TestInsertMultipleRows) {
    Parser parser;
    auto stmt = ParseSQL(parser, "INSERT INTO users VALUES (1, 'A'), (2, 'B'), (3, 'C')");
    
    const auto& insert = std::get<InsertStmt>(stmt->data);
    ASSERT_EQ(insert.values.size(), 3u);
}

TEST(TestUpdateSimple) {
    Parser parser;
    auto stmt = ParseSQL(parser, "UPDATE users SET name = 'Charlie'");
    
    ASSERT_EQ(stmt->type, StmtType::UPDATE);
    const auto& update = std::get<UpdateStmt>(stmt->data);
    ASSERT_EQ(update.table_name, "users");
    ASSERT_EQ(update.updates.size(), 1u);
    ASSERT_EQ(update.updates[0].column_name, "name");
    ASSERT(update.where_clause == nullptr);
}

TEST(TestUpdateWithWhere) {
    Parser parser;
    auto stmt = ParseSQL(parser, "UPDATE users SET age = 30 WHERE id = 1");
    
    const auto& update = std::get<UpdateStmt>(stmt->data);
    ASSERT(update.where_clause != nullptr);
}

TEST(TestDeleteSimple) {
    Parser parser;
    auto stmt = ParseSQL(parser, "DELETE FROM users");
    
    ASSERT_EQ(stmt->type, StmtType::DELETE_STMT);
    const auto& del = std::get<DeleteStmt>(stmt->data);
    ASSERT_EQ(del.table_name, "users");
    ASSERT(del.where_clause == nullptr);
}

TEST(TestDeleteWithWhere) {
    Parser parser;
    auto stmt = ParseSQL(parser, "DELETE FROM users WHERE id > 10");
    
    const auto& del = std::get<DeleteStmt>(stmt->data);
    ASSERT(del.where_clause != nullptr);
}

// ============================================================================
// SELECT 测试
// ============================================================================

TEST(TestSelectStar) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM users");
    
    ASSERT_EQ(stmt->type, StmtType::SELECT);
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.select_list.size(), 1u);
    ASSERT(select.select_list[0].is_star);
    ASSERT_EQ(select.from_tables.size(), 1u);
    ASSERT_EQ(select.from_tables[0].table_name, "users");
}

TEST(TestSelectColumns) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT id, name, age FROM users");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.select_list.size(), 3u);
}

TEST(TestSelectDistinct) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT DISTINCT name FROM users");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT(select.is_distinct);
}

TEST(TestSelectWithAlias) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT id AS user_id FROM users u");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.select_list[0].alias, "user_id");
    ASSERT_EQ(select.from_tables[0].alias, "u");
}

TEST(TestSelectWithWhere) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM users WHERE age > 18");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT(select.where_clause != nullptr);
}

TEST(TestSelectOrderBy) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM users ORDER BY name ASC, age DESC");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.order_by.size(), 2u);
    ASSERT(!select.order_by[0].is_desc);
    ASSERT(select.order_by[1].is_desc);
}

TEST(TestSelectLimitOffset) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM users LIMIT 10 OFFSET 20");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.limit, 10);
    ASSERT_EQ(select.offset, 20);
}

TEST(TestSelectJoin) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM users u INNER JOIN orders o ON u.id = o.user_id");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.joins.size(), 1u);
    ASSERT_EQ(select.joins[0].type, JoinType::INNER);
}

TEST(TestSelectLeftJoin) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM users LEFT JOIN orders ON users.id = orders.user_id");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.joins[0].type, JoinType::LEFT);
}

// ============================================================================
// 表达式测试
// ============================================================================

TEST(TestExpressionArithmetic) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT a + b * c FROM t");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT(select.select_list[0].expr != nullptr);
    ASSERT_EQ(select.select_list[0].expr->type, ExprType::BINARY_OP);
}

TEST(TestExpressionComparison) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM t WHERE a >= 10 AND b < 20");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT(select.where_clause != nullptr);
    ASSERT_EQ(select.where_clause->type, ExprType::BINARY_OP);
}

TEST(TestExpressionLike) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM t WHERE name LIKE 'A%'");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.where_clause->type, ExprType::LIKE);
}

TEST(TestExpressionIn) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM t WHERE id IN (1, 2, 3)");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.where_clause->type, ExprType::IN_LIST);
}

TEST(TestExpressionBetween) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM t WHERE age BETWEEN 18 AND 65");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.where_clause->type, ExprType::BETWEEN);
}

TEST(TestExpressionIsNull) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM t WHERE email IS NULL");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.where_clause->type, ExprType::IS_NULL);
}

TEST(TestExpressionIsNotNull) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT * FROM t WHERE name IS NOT NULL");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    const auto& is_null = std::get<IsNullExpr>(select.where_clause->data);
    ASSERT(is_null.is_not);
}

// ============================================================================
// 聚合函数测试
// ============================================================================

TEST(TestAggregateCount) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT COUNT(*) FROM users");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.select_list[0].expr->type, ExprType::FUNCTION_CALL);
}

TEST(TestAggregateSumAvgMinMax) {
    Parser parser;
    auto stmt = ParseSQL(parser, "SELECT SUM(amount), AVG(price), MIN(id), MAX(id) FROM orders");
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.select_list.size(), 4u);
}

// ============================================================================
// DCL 测试
// ============================================================================

TEST(TestCreateUser) {
    Parser parser;
    auto stmt = ParseSQL(parser, "CREATE USER 'admin' WITH PASSWORD 'secret'");
    
    ASSERT_EQ(stmt->type, StmtType::CREATE_USER);
    const auto& user = std::get<CreateUserStmt>(stmt->data);
    ASSERT_EQ(user.username, "admin");
    ASSERT_EQ(user.password, "secret");
}

TEST(TestDropUser) {
    Parser parser;
    auto stmt = ParseSQL(parser, "DROP USER 'guest'");
    
    ASSERT_EQ(stmt->type, StmtType::DROP_USER);
    const auto& user = std::get<DropUserStmt>(stmt->data);
    ASSERT_EQ(user.username, "guest");
}

TEST(TestGrant) {
    Parser parser;
    auto stmt = ParseSQL(parser, "GRANT SELECT, INSERT ON users TO 'reader'");
    
    ASSERT_EQ(stmt->type, StmtType::GRANT);
    const auto& grant = std::get<GrantStmt>(stmt->data);
    ASSERT_EQ(grant.privileges.size(), 2u);
    ASSERT_EQ(grant.table_name, "users");
    ASSERT_EQ(grant.username, "reader");
}

TEST(TestRevoke) {
    Parser parser;
    auto stmt = ParseSQL(parser, "REVOKE DELETE ON users FROM 'guest'");
    
    ASSERT_EQ(stmt->type, StmtType::REVOKE);
    const auto& revoke = std::get<RevokeStmt>(stmt->data);
    ASSERT_EQ(revoke.privileges[0], PrivilegeType::DELETE_PRIV);
}

// ============================================================================
// TCL 测试
// ============================================================================

TEST(TestBegin) {
    Parser parser;
    auto stmt = ParseSQL(parser, "BEGIN");
    ASSERT_EQ(stmt->type, StmtType::BEGIN_TXN);
}

TEST(TestBeginTransaction) {
    Parser parser;
    auto stmt = ParseSQL(parser, "BEGIN TRANSACTION");
    ASSERT_EQ(stmt->type, StmtType::BEGIN_TXN);
}

TEST(TestCommit) {
    Parser parser;
    auto stmt = ParseSQL(parser, "COMMIT");
    ASSERT_EQ(stmt->type, StmtType::COMMIT);
}

TEST(TestRollback) {
    Parser parser;
    auto stmt = ParseSQL(parser, "ROLLBACK");
    ASSERT_EQ(stmt->type, StmtType::ROLLBACK);
}

// ============================================================================
// 错误处理测试
// ============================================================================

TEST(TestParseError) {
    Parser parser;
    auto stmt = parser.Parse("SELEC * FROM users");  // 拼写错误
    ASSERT(stmt == nullptr);
    ASSERT(parser.HasError());
}

TEST(TestSyntaxError) {
    Parser parser;
    auto stmt = parser.Parse("SELECT FROM users");  // 缺少列
    ASSERT(stmt == nullptr);
    ASSERT(parser.HasError());
}

// ============================================================================
// 复杂查询测试
// ============================================================================

TEST(TestComplexQuery) {
    Parser parser;
    auto stmt = ParseSQL(parser,
        "SELECT u.id, u.name, COUNT(o.id) AS order_count "
        "FROM users u "
        "LEFT JOIN orders o ON u.id = o.user_id "
        "WHERE u.age >= 18 "
        "ORDER BY order_count DESC "
        "LIMIT 10"
    );
    ASSERT_EQ(stmt->type, StmtType::SELECT);
    
    const auto& select = std::get<SelectStmt>(stmt->data);
    ASSERT_EQ(select.select_list.size(), 3u);
    ASSERT_EQ(select.from_tables.size(), 1u);
    ASSERT_EQ(select.joins.size(), 1u);
    ASSERT(select.where_clause != nullptr);
    ASSERT_EQ(select.order_by.size(), 1u);
    ASSERT_EQ(select.limit, 10);
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    MiniDB SQL Parser Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::cout << "[DDL Tests]" << std::endl;
    RUN_TEST(TestCreateTableSimple);
    RUN_TEST(TestCreateTableIfNotExists);
    RUN_TEST(TestDropTable);
    RUN_TEST(TestDropTableIfExists);
    RUN_TEST(TestAlterTableAddColumn);
    RUN_TEST(TestCreateIndex);
    std::cout << std::endl;

    std::cout << "[DML Tests]" << std::endl;
    RUN_TEST(TestInsertSimple);
    RUN_TEST(TestInsertWithColumns);
    RUN_TEST(TestInsertMultipleRows);
    RUN_TEST(TestUpdateSimple);
    RUN_TEST(TestUpdateWithWhere);
    RUN_TEST(TestDeleteSimple);
    RUN_TEST(TestDeleteWithWhere);
    std::cout << std::endl;

    std::cout << "[SELECT Tests]" << std::endl;
    RUN_TEST(TestSelectStar);
    RUN_TEST(TestSelectColumns);
    RUN_TEST(TestSelectDistinct);
    RUN_TEST(TestSelectWithAlias);
    RUN_TEST(TestSelectWithWhere);
    RUN_TEST(TestSelectOrderBy);
    RUN_TEST(TestSelectLimitOffset);
    RUN_TEST(TestSelectJoin);
    RUN_TEST(TestSelectLeftJoin);
    std::cout << std::endl;

    std::cout << "[Expression Tests]" << std::endl;
    RUN_TEST(TestExpressionArithmetic);
    RUN_TEST(TestExpressionComparison);
    RUN_TEST(TestExpressionLike);
    RUN_TEST(TestExpressionIn);
    RUN_TEST(TestExpressionBetween);
    RUN_TEST(TestExpressionIsNull);
    RUN_TEST(TestExpressionIsNotNull);
    std::cout << std::endl;

    std::cout << "[Aggregate Tests]" << std::endl;
    RUN_TEST(TestAggregateCount);
    RUN_TEST(TestAggregateSumAvgMinMax);
    std::cout << std::endl;

    std::cout << "[DCL Tests]" << std::endl;
    RUN_TEST(TestCreateUser);
    RUN_TEST(TestDropUser);
    RUN_TEST(TestGrant);
    RUN_TEST(TestRevoke);
    std::cout << std::endl;

    std::cout << "[TCL Tests]" << std::endl;
    RUN_TEST(TestBegin);
    RUN_TEST(TestBeginTransaction);
    RUN_TEST(TestCommit);
    RUN_TEST(TestRollback);
    std::cout << std::endl;

    std::cout << "[Error Handling Tests]" << std::endl;
    RUN_TEST(TestParseError);
    RUN_TEST(TestSyntaxError);
    std::cout << std::endl;

    std::cout << "[Complex Query Tests]" << std::endl;
    RUN_TEST(TestComplexQuery);
    std::cout << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << tests_passed << " passed, " 
              << tests_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
