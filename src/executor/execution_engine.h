#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>

#include "common/types.h"
#include "parser/parser.h"
#include "catalog/catalog.h"
#include "executor/executor.h"
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

private:
    Catalog* catalog_;
    BufferPoolManager* bpm_;
    TransactionManager* txn_mgr_;
    Parser parser_;
    
    // Executors for different statement types
    ExecutionResult ExecuteCreateTable(const CreateTableStmt& stmt);
    ExecutionResult ExecuteDropTable(const DropTableStmt& stmt);
    ExecutionResult ExecuteAlterTable(const AlterTableStmt& stmt);
    ExecutionResult ExecuteInsert(const InsertStmt& stmt);
    ExecutionResult ExecuteSelect(const SelectStmt& stmt);
    ExecutionResult ExecuteDelete(const DeleteStmt& stmt);
    ExecutionResult ExecuteUpdate(const UpdateStmt& stmt);
    
    // DCL
    ExecutionResult ExecuteCreateUser(const CreateUserStmt& stmt);
    ExecutionResult ExecuteDropUser(const DropUserStmt& stmt);
    
    // TCL
    ExecutionResult ExecuteBegin(const BeginStmt& stmt);
    ExecutionResult ExecuteCommit(const CommitStmt& stmt);
    ExecutionResult ExecuteRollback(const RollbackStmt& stmt);

    // Transaction state
    Transaction* current_txn_ = nullptr;
    
    // Temporary expressions (owned by ExecutionEngine, cleared per execution)
    std::vector<std::unique_ptr<Expression>> temporary_exprs_;
    
    // Helpers
    std::unique_ptr<Operator> BuildExecutionPlan(const SelectStmt& stmt, ExecutorContext* ctx);
    std::unique_ptr<Operator> BuildTableRefOperator(const TableRef& table_ref, ExecutorContext* ctx);
    std::unique_ptr<Expression> ConvertExpression(const Expression& ast_expr, const OutputSchema* schema);
};

} // namespace minidb
