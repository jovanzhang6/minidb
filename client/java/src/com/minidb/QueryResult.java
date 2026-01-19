package com.minidb;

import java.util.List;

/**
 * 查询结果
 */
public class QueryResult {
    private final List<ColumnInfo> columns;
    private final List<List<Object>> rows;
    private final int rowCount;
    private final int affectedRows;
    private final String message;
    
    public QueryResult(List<ColumnInfo> columns, List<List<Object>> rows, 
                       int rowCount, int affectedRows, String message) {
        this.columns = columns;
        this.rows = rows;
        this.rowCount = rowCount;
        this.affectedRows = affectedRows;
        this.message = message;
    }
    
    /**
     * 获取列信息
     */
    public List<ColumnInfo> getColumns() {
        return columns;
    }
    
    /**
     * 获取所有行
     */
    public List<List<Object>> getRows() {
        return rows;
    }
    
    /**
     * 获取指定行
     */
    public List<Object> getRow(int index) {
        return rows.get(index);
    }
    
    /**
     * 获取行数
     */
    public int getRowCount() {
        return rowCount;
    }
    
    /**
     * 获取影响行数
     */
    public int getAffectedRows() {
        return affectedRows;
    }
    
    /**
     * 获取消息
     */
    public String getMessage() {
        return message;
    }
    
    /**
     * 获取指定单元格的值
     */
    public Object getValue(int row, int col) {
        return rows.get(row).get(col);
    }
    
    /**
     * 按列名获取值
     */
    public Object getValue(int row, String columnName) {
        int colIndex = getColumnIndex(columnName);
        if (colIndex < 0) {
            throw new IllegalArgumentException("列不存在: " + columnName);
        }
        return rows.get(row).get(colIndex);
    }
    
    /**
     * 获取整数值
     */
    public int getInt(int row, String columnName) {
        Object val = getValue(row, columnName);
        if (val == null) return 0;
        if (val instanceof Number) return ((Number) val).intValue();
        return Integer.parseInt(val.toString());
    }
    
    /**
     * 获取长整数值
     */
    public long getLong(int row, String columnName) {
        Object val = getValue(row, columnName);
        if (val == null) return 0;
        if (val instanceof Number) return ((Number) val).longValue();
        return Long.parseLong(val.toString());
    }
    
    /**
     * 获取浮点值
     */
    public double getDouble(int row, String columnName) {
        Object val = getValue(row, columnName);
        if (val == null) return 0.0;
        if (val instanceof Number) return ((Number) val).doubleValue();
        return Double.parseDouble(val.toString());
    }
    
    /**
     * 获取字符串值
     */
    public String getString(int row, String columnName) {
        Object val = getValue(row, columnName);
        if (val == null) return null;
        return val.toString();
    }
    
    /**
     * 获取列索引
     */
    public int getColumnIndex(String columnName) {
        for (int i = 0; i < columns.size(); i++) {
            if (columns.get(i).getName().equalsIgnoreCase(columnName)) {
                return i;
            }
        }
        return -1;
    }
    
    /**
     * 是否有数据
     */
    public boolean hasData() {
        return rowCount > 0;
    }
    
    /**
     * 是否为空
     */
    public boolean isEmpty() {
        return rowCount == 0;
    }
}
