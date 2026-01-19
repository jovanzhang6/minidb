/**
 * @file lexer.cpp
 * @brief 手写SQL词法分析器实现
 */

#include "parser/lexer.h"
#include <cctype>
#include <algorithm>

namespace minidb {

// 关键字映射表初始化
std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    // DDL
    {"CREATE", TokenType::CREATE},
    {"DROP", TokenType::DROP},
    {"ALTER", TokenType::ALTER},
    {"TABLE", TokenType::TABLE},
    {"INDEX", TokenType::INDEX},
    {"COLUMN", TokenType::COLUMN},
    {"ADD", TokenType::ADD},
    {"RENAME", TokenType::RENAME},
    {"TO", TokenType::TO},
    {"TYPE", TokenType::TYPE},
    {"UNIQUE", TokenType::UNIQUE},
    {"IF", TokenType::IF},
    {"EXISTS", TokenType::EXISTS},
    
    // DML
    {"INSERT", TokenType::INSERT},
    {"INTO", TokenType::INTO},
    {"VALUES", TokenType::VALUES},
    {"UPDATE", TokenType::UPDATE},
    {"SET", TokenType::SET},
    {"DELETE", TokenType::DELETE},
    
    // DQL
    {"SELECT", TokenType::SELECT},
    {"FROM", TokenType::FROM},
    {"WHERE", TokenType::WHERE},
    {"AS", TokenType::AS},
    {"DISTINCT", TokenType::DISTINCT},
    {"ALL", TokenType::ALL},
    {"ORDER", TokenType::ORDER},
    {"BY", TokenType::BY},
    {"ASC", TokenType::ASC},
    {"DESC", TokenType::DESC},
    {"LIMIT", TokenType::LIMIT},
    {"OFFSET", TokenType::OFFSET},
    {"GROUP", TokenType::GROUP},
    {"HAVING", TokenType::HAVING},
    
    // JOIN
    {"JOIN", TokenType::JOIN},
    {"INNER", TokenType::INNER},
    {"LEFT", TokenType::LEFT},
    {"RIGHT", TokenType::RIGHT},
    {"FULL", TokenType::FULL},
    {"OUTER", TokenType::OUTER},
    {"CROSS", TokenType::CROSS},
    {"ON", TokenType::ON},
    
    // DCL
    {"USER", TokenType::USER},
    {"WITH", TokenType::WITH},
    {"PASSWORD", TokenType::PASSWORD},
    {"GRANT", TokenType::GRANT},
    {"REVOKE", TokenType::REVOKE},
    
    // TCL
    {"BEGIN", TokenType::BEGIN_KW},
    {"COMMIT", TokenType::COMMIT},
    {"ROLLBACK", TokenType::ROLLBACK},
    {"TRANSACTION", TokenType::TRANSACTION},
    
    // 数据类型
    {"INT", TokenType::INT_TYPE},
    {"INTEGER", TokenType::INT_TYPE},
    {"FLOAT", TokenType::FLOAT_TYPE},
    {"REAL", TokenType::FLOAT_TYPE},
    {"DOUBLE", TokenType::FLOAT_TYPE},
    {"TEXT", TokenType::TEXT_TYPE},
    {"VARCHAR", TokenType::TEXT_TYPE},
    {"STRING", TokenType::TEXT_TYPE},
    
    // 视图
    {"VIEW", TokenType::VIEW},
    
    // 约束
    {"PRIMARY", TokenType::PRIMARY},
    {"KEY", TokenType::KEY},
    {"NOT", TokenType::NOT},
    {"NULL", TokenType::NULL_KW},
    {"DEFAULT", TokenType::DEFAULT},
    
    // 逻辑
    {"AND", TokenType::AND},
    {"OR", TokenType::OR},
    
    // 比较
    {"LIKE", TokenType::LIKE},
    {"IN", TokenType::IN},
    {"BETWEEN", TokenType::BETWEEN},
    {"IS", TokenType::IS},
    
    // 聚合
    {"COUNT", TokenType::COUNT},
    {"SUM", TokenType::SUM},
    {"AVG", TokenType::AVG},
    {"MIN", TokenType::MIN},
    {"MAX", TokenType::MAX},
};

Lexer::Lexer(const std::string& sql) : sql_(sql) {}

Token Lexer::NextToken() {
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_token_;
    }
    return ScanToken();
}

Token Lexer::PeekToken() {
    if (!has_peeked_) {
        peeked_token_ = ScanToken();
        has_peeked_ = true;
    }
    return peeked_token_;
}

char Lexer::Peek() const {
    if (IsAtEnd()) return '\0';
    return sql_[current_];
}

char Lexer::PeekNext() const {
    if (current_ + 1 >= sql_.size()) return '\0';
    return sql_[current_ + 1];
}

char Lexer::Advance() {
    char c = sql_[current_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::IsAtEnd() const {
    return current_ >= sql_.size();
}

bool Lexer::Match(char expected) {
    if (IsAtEnd()) return false;
    if (sql_[current_] != expected) return false;
    Advance();
    return true;
}

void Lexer::SkipWhitespace() {
    while (!IsAtEnd()) {
        char c = Peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                Advance();
                break;
            case '-':
                if (PeekNext() == '-') {
                    // 行注释
                    while (!IsAtEnd() && Peek() != '\n') {
                        Advance();
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

Token Lexer::ScanToken() {
    SkipWhitespace();
    
    start_ = current_;
    token_column_ = column_;
    
    if (IsAtEnd()) {
        return MakeToken(TokenType::END_OF_FILE);
    }
    
    char c = Advance();
    
    // 标识符或关键字
    if (std::isalpha(c) || c == '_') {
        return ScanIdentifierOrKeyword();
    }
    
    // 数字
    if (std::isdigit(c)) {
        return ScanNumber();
    }
    
    // 字符串
    if (c == '\'') {
        return ScanString();
    }
    
    // 运算符和标点
    switch (c) {
        case '(': return MakeToken(TokenType::LPAREN);
        case ')': return MakeToken(TokenType::RPAREN);
        case ',': return MakeToken(TokenType::COMMA);
        case ';': return MakeToken(TokenType::SEMICOLON);
        case '.': return MakeToken(TokenType::DOT);
        case '+': return MakeToken(TokenType::PLUS);
        case '-': return MakeToken(TokenType::MINUS);
        case '*': return MakeToken(TokenType::STAR);
        case '/': return MakeToken(TokenType::SLASH);
        case '%': return MakeToken(TokenType::PERCENT);
        case '=': return MakeToken(TokenType::EQ);
        case '<':
            if (Match('=')) return MakeToken(TokenType::LE);
            if (Match('>')) return MakeToken(TokenType::NE);
            return MakeToken(TokenType::LT);
        case '>':
            if (Match('=')) return MakeToken(TokenType::GE);
            return MakeToken(TokenType::GT);
        case '!':
            if (Match('=')) return MakeToken(TokenType::NE);
            return ErrorToken("Unexpected character '!'");
    }
    
    return ErrorToken("Unexpected character");
}

Token Lexer::ScanIdentifierOrKeyword() {
    while (!IsAtEnd() && (std::isalnum(Peek()) || Peek() == '_')) {
        Advance();
    }
    
    std::string text = sql_.substr(start_, current_ - start_);
    TokenType type = LookupKeyword(text);
    
    Token token = MakeToken(type, text);
    return token;
}

Token Lexer::ScanNumber() {
    bool is_float = false;
    
    while (!IsAtEnd() && std::isdigit(Peek())) {
        Advance();
    }
    
    // 小数点
    if (Peek() == '.' && std::isdigit(PeekNext())) {
        is_float = true;
        Advance();  // 消费 '.'
        while (!IsAtEnd() && std::isdigit(Peek())) {
            Advance();
        }
    }
    
    std::string text = sql_.substr(start_, current_ - start_);
    Token token = MakeToken(is_float ? TokenType::FLOAT_LITERAL : TokenType::INTEGER_LITERAL, text);
    
    if (is_float) {
        token.float_value = std::stod(text);
    } else {
        token.int_value = std::stoll(text);
    }
    
    return token;
}

Token Lexer::ScanString() {
    std::string value;
    
    while (!IsAtEnd() && Peek() != '\'') {
        if (Peek() == '\\' && PeekNext() != '\0') {
            Advance();  // 跳过反斜杠
            char escaped = Advance();
            switch (escaped) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '\\': value += '\\'; break;
                case '\'': value += '\''; break;
                default:   value += escaped; break;
            }
        } else {
            value += Advance();
        }
    }
    
    if (IsAtEnd()) {
        return ErrorToken("Unterminated string");
    }
    
    Advance();  // 消费闭合的引号
    
    Token token = MakeToken(TokenType::STRING_LITERAL);
    token.string_value = value;
    return token;
}

Token Lexer::MakeToken(TokenType type) {
    std::string text = sql_.substr(start_, current_ - start_);
    return Token(type, text, line_, token_column_);
}

Token Lexer::MakeToken(TokenType type, const std::string& value) {
    Token token(type, value, line_, token_column_);
    return token;
}

Token Lexer::ErrorToken(const std::string& message) {
    Token token(TokenType::ERROR, message, line_, token_column_);
    return token;
}

TokenType Lexer::LookupKeyword(const std::string& identifier) {
    // 转换为大写进行查找
    std::string upper = identifier;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    auto it = keywords_.find(upper);
    if (it != keywords_.end()) {
        return it->second;
    }
    return TokenType::IDENTIFIER;
}

std::string TokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::ERROR: return "ERROR";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INTEGER_LITERAL: return "INTEGER";
        case TokenType::FLOAT_LITERAL: return "FLOAT";
        case TokenType::STRING_LITERAL: return "STRING";
        
        case TokenType::CREATE: return "CREATE";
        case TokenType::DROP: return "DROP";
        case TokenType::ALTER: return "ALTER";
        case TokenType::TABLE: return "TABLE";
        case TokenType::INDEX: return "INDEX";
        case TokenType::COLUMN: return "COLUMN";
        case TokenType::ADD: return "ADD";
        case TokenType::RENAME: return "RENAME";
        case TokenType::TO: return "TO";
        case TokenType::TYPE: return "TYPE";
        case TokenType::UNIQUE: return "UNIQUE";
        case TokenType::IF: return "IF";
        case TokenType::EXISTS: return "EXISTS";
        
        case TokenType::INSERT: return "INSERT";
        case TokenType::INTO: return "INTO";
        case TokenType::VALUES: return "VALUES";
        case TokenType::UPDATE: return "UPDATE";
        case TokenType::SET: return "SET";
        case TokenType::DELETE: return "DELETE";
        
        case TokenType::SELECT: return "SELECT";
        case TokenType::FROM: return "FROM";
        case TokenType::WHERE: return "WHERE";
        case TokenType::AS: return "AS";
        case TokenType::DISTINCT: return "DISTINCT";
        case TokenType::ALL: return "ALL";
        case TokenType::ORDER: return "ORDER";
        case TokenType::BY: return "BY";
        case TokenType::ASC: return "ASC";
        case TokenType::DESC: return "DESC";
        case TokenType::LIMIT: return "LIMIT";
        case TokenType::OFFSET: return "OFFSET";
        case TokenType::GROUP: return "GROUP";
        case TokenType::HAVING: return "HAVING";
        
        case TokenType::JOIN: return "JOIN";
        case TokenType::INNER: return "INNER";
        case TokenType::LEFT: return "LEFT";
        case TokenType::RIGHT: return "RIGHT";
        case TokenType::FULL: return "FULL";
        case TokenType::OUTER: return "OUTER";
        case TokenType::CROSS: return "CROSS";
        case TokenType::ON: return "ON";
        
        case TokenType::USER: return "USER";
        case TokenType::WITH: return "WITH";
        case TokenType::PASSWORD: return "PASSWORD";
        case TokenType::GRANT: return "GRANT";
        case TokenType::REVOKE: return "REVOKE";
        
        case TokenType::BEGIN_KW: return "BEGIN";
        case TokenType::COMMIT: return "COMMIT";
        case TokenType::ROLLBACK: return "ROLLBACK";
        case TokenType::TRANSACTION: return "TRANSACTION";
        
        case TokenType::INT_TYPE: return "INT";
        case TokenType::FLOAT_TYPE: return "FLOAT";
        case TokenType::TEXT_TYPE: return "TEXT";
        
        case TokenType::PRIMARY: return "PRIMARY";
        case TokenType::KEY: return "KEY";
        case TokenType::NOT: return "NOT";
        case TokenType::NULL_KW: return "NULL";
        case TokenType::DEFAULT: return "DEFAULT";
        
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        
        case TokenType::LIKE: return "LIKE";
        case TokenType::IN: return "IN";
        case TokenType::BETWEEN: return "BETWEEN";
        case TokenType::IS: return "IS";
        
        case TokenType::COUNT: return "COUNT";
        case TokenType::SUM: return "SUM";
        case TokenType::AVG: return "AVG";
        case TokenType::MIN: return "MIN";
        case TokenType::MAX: return "MAX";
        
        case TokenType::EQ: return "=";
        case TokenType::NE: return "<>";
        case TokenType::LT: return "<";
        case TokenType::LE: return "<=";
        case TokenType::GT: return ">";
        case TokenType::GE: return ">=";
        
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::STAR: return "*";
        case TokenType::SLASH: return "/";
        case TokenType::PERCENT: return "%";
        
        case TokenType::LPAREN: return "(";
        case TokenType::RPAREN: return ")";
        case TokenType::COMMA: return ",";
        case TokenType::SEMICOLON: return ";";
        case TokenType::DOT: return ".";
        
        default: return "UNKNOWN";
    }
}

}  // namespace minidb
