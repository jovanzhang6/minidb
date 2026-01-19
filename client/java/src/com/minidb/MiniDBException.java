package com.minidb;

/**
 * MiniDB 基础异常类
 */
public class MiniDBException extends Exception {
    public MiniDBException(String message) {
        super(message);
    }
    
    public MiniDBException(String message, Throwable cause) {
        super(message, cause);
    }
}

/**
 * 认证异常
 */
class AuthenticationException extends MiniDBException {
    public AuthenticationException(String message) {
        super(message);
    }
}

/**
 * 查询异常
 */
class QueryException extends MiniDBException {
    public QueryException(String message) {
        super(message);
    }
}
