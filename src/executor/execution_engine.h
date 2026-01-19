#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>

#include "common/types.h"
#include "parser/parser.h"
#include "catalog/catalog.h"
#include "executor/executor.h"
#include "executor/index_scan.h"
#include "btree/btree_index.h"
#include "txn/transaction_manager.h"

namespace minidb {

/**
 * @brief Execution result
 */
struct ExecutionResult {
    bool success;
    std::string message;  // Error message or success info
    std::unique_ptr<OutputSchema> schema;
    std::vector<Tuple> tuples;
    
    static ExecutionResult Success(const std::string& msg = "Success") {
        return {true, msg, nullptr, {}};
    }
    
    static ExecutionResult Fail(const std::string& msg) {
        return {false, msg, nullptr, {}};
    }
    
    static ExecutionResult ResultSet(std::unique_ptr<OutputSchema> schema, std::vector<Tuple> tuples) {
        return {true, "", std::move(schema), std::move(tuples)};
    }
};

/**
 * @brief Execution Engine
 * 
 * Entrance for executing SQL statements.
 * It coordinates Parser, Binder (implicit), Optimizer (none), and Executor.
 */
class ExecutionEngine {
public:
    ExecutionEngine(Catalog* catalog, BufferPoolManager* bpm, TransactionManager* txn_mgr);
    
    /**
     * @brief Execute a SQL statement
     */
    ExecutionResult Execute(const std::string& sql);
    
    /**
     * @brief Set current user for permission checking
     */
    void SetCurrentUser(const UserInfo& user) { current_user_ = user; has_user_ = true; }
    
    /**
     * @brief Clear current user (logout)
     */
    void ClearCurrentUser() { has_user_ = false; }
    
    /**
     * @brief Check if a user is logged in
     */
    bool HasCurrentUser() const { return has_user_; }
    
    /**
     * @brief Get current user info
     */
    const UserInfo& GetCurrentUser() const { return current_user_; }

private:
    Catalog* catalog_;
    BufferPoolManager* bpm_;
    TransactionManager* txn_mgr_;
    Parser parser_;
    
    // Current logged-in user
    UserInfo current_user_;
    bool has_user_ = false;
    
    // Permission check helpers
    bool IsAdmin() const { return has_user_ && current_user_.is_admin; }
    ExecutionResult CheckDDLPermission();  // For CREATE/DROP/ALTER TABLE
    ExecutionResult CheckDCLPermission();  // For CREATE/DROP USER, GRANT, REVOKE
    ExecutionResult CheckDMLPermission(const std::string& table_name, PrivilegeType required);
    
    // Executors for different statement types
    ExecutionResult ExecuteCreateTable(const CreateTableStmt& stmt);
    ExecutionResult ExecuteDropTable(const DropTableStmt& stmt);
    ExecutionResult ExecuteAlterTable(const AlterTableStmt& stmt);
    ExecutionResult ExecuteCreateIndex(const CreateIndexStmt& stmt);
    ExecutionResult ExecuteDropIndex(const DropIndexStmt& stmt);
    ExecutionResult ExecuteCreateView(const CreateViewStmt& stmt, const std::string& original_sql);
    ExecutionResult ExecuteDropView(const DropViewStmt& stmt);
    ExecutionResult ExecuteInsert(const InsertStmt& stmt);
    ExecutionResult ExecuteSelect(const SelectStmt& stmt);
    ExecutionResult ExecuteDelete(const DeleteStmt& stmt);
    ExecutionResult ExecuteUpdate(const UpdateStmt& stmt);
    
    // DCL
    ExecutionResult ExecuteCreateUser(const CreateUserStmt& stmt);
    ExecutionResult ExecuteDropUser(const DropUserStmt& stmt);
    ExecutionResult ExecuteGrant(const GrantStmt& stmt);
    ExecutionResult ExecuteRevoke(const RevokeStmt& stmt);
    
    // TCL
    ExecutionResult ExecuteBegin(const BeginStmt& stmt);
    ExecutionResult ExecuteCommit(const CommitStmt& stmt);
    ExecutionResult ExecuteRollback(const RollbackStmt& stmt);

    // Transaction state
    Transaction* current_txn_ = nullptr;
    
    // Temporary expressions (owned by ExecutionEngine, cleared per execution)
    std::vector<std::unique_ptr<Expression>> temporary_exprs_;
    
    // Temporary parsed statements for view expansion (cleared per execution)
    std::vector<std::unique_ptr<Statement>> temporary_view_stmts_;
    
    // Helpers
    std::unique_ptr<Operator> BuildExecutionPlan(const SelectStmt& stmt, ExecutorContext* ctx);
    std::unique_ptr<Operator> BuildTableRefOperator(const TableRef& table_ref, ExecutorContext* ctx);
    std::unique_ptr<Expression> ConvertExpression(const Expression& ast_expr, const OutputSchema* schema);
    
    // Index optimization helpers
    struct IndexableCondition {
        std::string table_name;
        std::string column_name;
        Value value;
        bool valid = false;
    };
    IndexableCondition TryExtractIndexableCondition(const Expression* where_clause) const;
    
    // Owned indexes for current query (cleaned up after query)
    std::vector<std::unique_ptr<BTreeIndex>> query_indexes_;
    
    // Convert AST privilege type to catalog privilege type
    PrivilegeType ConvertPrivilegeType(AstPrivilegeType ast_type);
};

} // namespace minidb
