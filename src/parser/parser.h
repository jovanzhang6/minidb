/**
 * @file parser.h
 * @brief SQL递归下降解析器
 * 
 * 将SQL字符串解析为AST的解析器接口
 */

#pragma once

#include <string>
#include <memory>
#include "parser/ast.h"
#include "parser/lexer.h"

namespace minidb {

/**
 * @brief SQL递归下降解析器
 * 
 * 手写解析器，无需依赖Flex/Bison
 */
class Parser {
public:
    /**
     * @brief 解析SQL语句
     * @param sql SQL语句字符串
     * @return 解析成功返回AST，失败返回nullptr
     */
    std::unique_ptr<Statement> Parse(const std::string& sql);
    
    /**
     * @brief 获取最后一次解析的错误信息
     */
    const std::string& GetErrorMessage() const { return error_message_; }
    
    /**
     * @brief 检查解析是否成功
     */
    bool HasError() const { return !error_message_.empty(); }

private:
    // Token操作
    Token Advance();
    Token Peek() const;
    Token Previous() const;
    bool Check(TokenType type) const;
    bool Match(TokenType type);
    bool Match(std::initializer_list<TokenType> types);
    Token Consume(TokenType type, const std::string& message);
    bool IsAtEnd() const;
    
    // 错误处理
    void Error(const std::string& message);
    void Error(const Token& token, const std::string& message);
    void Synchronize();  // 错误恢复
    
    // 语句解析
    std::unique_ptr<Statement> ParseStatement();
    
    // DDL解析
    std::unique_ptr<Statement> ParseCreateStatement();
    std::unique_ptr<Statement> ParseCreateTable();
    std::unique_ptr<Statement> ParseCreateIndex();
    std::unique_ptr<Statement> ParseCreateUser();
    std::unique_ptr<Statement> ParseDropStatement();
    std::unique_ptr<Statement> ParseDropTable();
    std::unique_ptr<Statement> ParseDropUser();
    std::unique_ptr<Statement> ParseAlterTable();
    
    // DML解析
    std::unique_ptr<Statement> ParseInsert();
    std::unique_ptr<Statement> ParseUpdate();
    std::unique_ptr<Statement> ParseDelete();
    
    // DQL解析
    std::unique_ptr<Statement> ParseSelect();
    std::vector<SelectItem> ParseSelectList();
    SelectItem ParseSelectItem();
    std::vector<TableRef> ParseFromClause();
    TableRef ParseTableRef();
    std::vector<JoinClause> ParseJoinClauses();
    JoinClause ParseJoinClause();
    std::vector<OrderByItem> ParseOrderBy();
    OrderByItem ParseOrderByItem();
    
    // DCL解析
    std::unique_ptr<Statement> ParseGrant();
    std::unique_ptr<Statement> ParseRevoke();
    std::vector<PrivilegeType> ParsePrivilegeList();
    
    // TCL解析
    std::unique_ptr<Statement> ParseBegin();
    std::unique_ptr<Statement> ParseCommit();
    std::unique_ptr<Statement> ParseRollback();
    
    // 辅助解析
    ColumnDef ParseColumnDef();
    DataType ParseDataType();
    std::vector<std::string> ParseColumnNameList();
    
    // 表达式解析（使用优先级爬升）
    std::unique_ptr<Expression> ParseExpression();
    std::unique_ptr<Expression> ParseOrExpr();
    std::unique_ptr<Expression> ParseAndExpr();
    std::unique_ptr<Expression> ParseNotExpr();
    std::unique_ptr<Expression> ParseComparisonExpr();
    std::unique_ptr<Expression> ParseAddExpr();
    std::unique_ptr<Expression> ParseMulExpr();
    std::unique_ptr<Expression> ParseUnaryExpr();
    std::unique_ptr<Expression> ParsePrimaryExpr();
    std::unique_ptr<Expression> ParseLiteral();
    std::unique_ptr<Expression> ParseColumnRefOrFunction();
    std::vector<std::unique_ptr<Expression>> ParseExpressionList();

private:
    std::unique_ptr<Lexer> lexer_;
    Token current_;
    Token previous_;
    std::string error_message_;
    bool had_error_ = false;
};

/**
 * @brief 打印AST（用于调试）
 */
void PrintAST(const Statement* stmt, int indent = 0);

/**
 * @brief 打印表达式（用于调试）
 */
void PrintExpression(const Expression* expr, int indent = 0);

}  // namespace minidb
