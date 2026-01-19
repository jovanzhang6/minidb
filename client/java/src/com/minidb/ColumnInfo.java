package com.minidb;

/**
 * 列信息
 */
public class ColumnInfo {
    private final String name;
    private final String type;
    
    public ColumnInfo(String name, String type) {
        this.name = name;
        this.type = type;
    }
    
    public String getName() {
        return name;
    }
    
    public String getType() {
        return type;
    }
    
    @Override
    public String toString() {
        return name + " (" + type + ")";
    }
}
