/**
 * @file lexer.h
 * @brief 手写SQL词法分析器
 * 
 * 简单高效的词法分析器，无需依赖Flex
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace minidb {

/**
 * @brief Token类型枚举
 */
enum class TokenType {
    // 特殊Token
    END_OF_FILE,
    ERROR,
    
    // 标识符和字面量
    IDENTIFIER,
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    
    // SQL关键字 - DDL
    CREATE,
    DROP,
    ALTER,
    TABLE,
    INDEX,
    COLUMN,
    ADD,
    RENAME,
    TO,
    TYPE,
    UNIQUE,
    IF,
    EXISTS,
    
    // SQL关键字 - DML
    INSERT,
    INTO,
    VALUES,
    UPDATE,
    SET,
    DELETE,
    
    // SQL关键字 - DQL
    SELECT,
    FROM,
    WHERE,
    AS,
    DISTINCT,
    ALL,
    ORDER,
    BY,
    ASC,
    DESC,
    LIMIT,
    OFFSET,
    GROUP,
    HAVING,
    
    // SQL关键字 - JOIN
    JOIN,
    INNER,
    LEFT,
    RIGHT,
    FULL,
    OUTER,
    CROSS,
    ON,
    
    // SQL关键字 - DCL
    USER,
    WITH,
    PASSWORD,
    GRANT,
    REVOKE,
    
    // SQL关键字 - TCL
    BEGIN_KW,
    COMMIT,
    ROLLBACK,
    TRANSACTION,
    
    // 数据类型
    INT_TYPE,
    FLOAT_TYPE,
    TEXT_TYPE,
    
    // 列约束
    PRIMARY,
    KEY,
    NOT,
    NULL_KW,
    DEFAULT,
    
    // 逻辑运算符
    AND,
    OR,
    
    // 比较运算符关键字
    LIKE,
    IN,
    BETWEEN,
    IS,
    
    // 聚合函数
    COUNT,
    SUM,
    AVG,
    MIN,
    MAX,
    
    // 比较运算符
    EQ,         // =
    NE,         // <> 或 !=
    LT,         // <
    LE,         // <=
    GT,         // >
    GE,         // >=
    
    // 算术运算符
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    PERCENT,    // %
    
    // 标点符号
    LPAREN,     // (
    RPAREN,     // )
    COMMA,      // ,
    SEMICOLON,  // ;
    DOT,        // .
};

/**
 * @brief Token结构
 */
struct Token {
    TokenType type;
    std::string value;      // 原始文本
    int line;               // 行号
    int column;             // 列号
    
    // 字面量值（根据类型使用）
    int64_t int_value = 0;
    double float_value = 0.0;
    std::string string_value;
    
    Token() : type(TokenType::END_OF_FILE), line(1), column(1) {}
    Token(TokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c) {}
};

/**
 * @brief SQL词法分析器
 */
class Lexer {
public:
    /**
     * @brief 构造函数
     * @param sql SQL语句字符串
     */
    explicit Lexer(const std::string& sql);
    
    /**
     * @brief 获取下一个Token
     * @return Token对象
     */
    Token NextToken();
    
    /**
     * @brief 查看下一个Token但不消费
     * @return Token对象
     */
    Token PeekToken();
    
    /**
     * @brief 获取当前行号
     */
    int GetLine() const { return line_; }
    
    /**
     * @brief 获取当前列号
     */
    int GetColumn() const { return column_; }

private:
    // 辅助函数
    char Peek() const;
    char PeekNext() const;
    char Advance();
    bool IsAtEnd() const;
    bool Match(char expected);
    
    void SkipWhitespace();
    void SkipComment();
    
    Token ScanToken();
    Token ScanIdentifierOrKeyword();
    Token ScanNumber();
    Token ScanString();
    
    Token MakeToken(TokenType type);
    Token MakeToken(TokenType type, const std::string& value);
    Token ErrorToken(const std::string& message);
    
    // 关键字查找
    TokenType LookupKeyword(const std::string& identifier);
    
private:
    std::string sql_;           // 输入SQL
    size_t start_ = 0;          // 当前Token开始位置
    size_t current_ = 0;        // 当前扫描位置
    int line_ = 1;              // 当前行号
    int column_ = 1;            // 当前列号
    int token_column_ = 1;      // Token开始列号
    
    Token peeked_token_;        // 预览的Token
    bool has_peeked_ = false;   // 是否有预览Token
    
    static std::unordered_map<std::string, TokenType> keywords_;
};

/**
 * @brief 将TokenType转换为字符串
 */
std::string TokenTypeToString(TokenType type);

}  // namespace minidb
