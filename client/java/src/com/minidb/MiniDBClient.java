package com.minidb;

import java.io.*;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.*;

/**
 * MiniDB Java 客户端
 * 
 * <pre>
 * 用法示例:
 * MiniDBClient client = new MiniDBClient("127.0.0.1", 9527, "root", "123456");
 * QueryResult result = client.execute("SELECT * FROM users");
 * for (int i = 0; i &lt; result.getRowCount(); i++) {
 *     System.out.println(result.getRow(i));
 * }
 * client.close();
 * </pre>
 */
public class MiniDBClient implements AutoCloseable {
    
    // Protocol constants
    private static final int PROTOCOL_MAGIC = 0x4D44;  // "MD"
    private static final int MAX_MESSAGE_SIZE = 16 * 1024 * 1024;  // 16MB
    private static final int DEFAULT_PORT = 9527;
    private static final int DEFAULT_TIMEOUT = 30000;  // 30 seconds
    
    private final String host;
    private final int port;
    private final String username;
    private final String password;
    private final int timeout;
    
    private Socket socket;
    private DataInputStream in;
    private DataOutputStream out;
    private String sessionId;
    private int seq = 0;
    private boolean connected = false;
    
    /**
     * 创建客户端并连接
     */
    public MiniDBClient(String host, int port, String username, String password) throws MiniDBException {
        this(host, port, username, password, DEFAULT_TIMEOUT);
    }
    
    /**
     * 创建客户端并连接
     */
    public MiniDBClient(String host, int port, String username, String password, int timeout) throws MiniDBException {
        this.host = host;
        this.port = port;
        this.username = username;
        this.password = password;
        this.timeout = timeout;
        
        connect();
    }
    
    /**
     * 连接到服务器
     */
    public void connect() throws MiniDBException {
        if (connected) {
            return;
        }
        
        try {
            socket = new Socket(host, port);
            socket.setSoTimeout(timeout);
            in = new DataInputStream(socket.getInputStream());
            out = new DataOutputStream(socket.getOutputStream());
            connected = true;
        } catch (SocketTimeoutException e) {
            throw new MiniDBException("连接超时: " + host + ":" + port, e);
        } catch (IOException e) {
            throw new MiniDBException("无法连接到服务器: " + host + ":" + port, e);
        }
        
        // Authenticate
        if (username != null && !username.isEmpty()) {
            authenticate();
        }
    }
    
    private void authenticate() throws MiniDBException {
        Map<String, Object> data = new HashMap<>();
        data.put("username", username);
        data.put("password", password);
        
        Map<String, Object> response = sendRequest("AUTH", data);
        
        Boolean success = (Boolean) response.get("success");
        if (success == null || !success) {
            String error = (String) response.get("error");
            throw new AuthenticationException(error != null ? error : "认证失败");
        }
        
        @SuppressWarnings("unchecked")
        Map<String, Object> respData = (Map<String, Object>) response.get("data");
        if (respData != null) {
            sessionId = (String) respData.get("session_id");
        }
    }
    
    /**
     * 执行 SQL 语句
     */
    public QueryResult execute(String sql) throws MiniDBException {
        if (!connected) {
            throw new MiniDBException("未连接到服务器");
        }
        
        Map<String, Object> data = new HashMap<>();
        data.put("session_id", sessionId);
        data.put("sql", sql);
        
        Map<String, Object> response = sendRequest("QUERY", data);
        
        Boolean success = (Boolean) response.get("success");
        if (success == null || !success) {
            String error = (String) response.get("error");
            throw new QueryException(error != null ? error : "查询失败");
        }
        
        return parseQueryResult(response);
    }
    
    /**
     * 发送心跳检测
     */
    public boolean ping() {
        if (!connected) {
            return false;
        }
        
        try {
            Map<String, Object> response = sendRequest("PING", new HashMap<>());
            Boolean success = (Boolean) response.get("success");
            return success != null && success;
        } catch (Exception e) {
            return false;
        }
    }
    
    /**
     * 关闭连接
     */
    @Override
    public void close() {
        if (!connected) {
            return;
        }
        
        try {
            if (sessionId != null) {
                Map<String, Object> data = new HashMap<>();
                data.put("session_id", sessionId);
                sendRequest("CLOSE", data);
            }
        } catch (Exception ignored) {
        }
        
        try {
            if (in != null) in.close();
            if (out != null) out.close();
            if (socket != null) socket.close();
        } catch (IOException ignored) {
        }
        
        socket = null;
        in = null;
        out = null;
        sessionId = null;
        connected = false;
    }
    
    /**
     * 是否已连接
     */
    public boolean isConnected() {
        return connected;
    }
    
    /**
     * 获取会话ID
     */
    public String getSessionId() {
        return sessionId;
    }
    
    private int nextSeq() {
        return ++seq;
    }
    
    private Map<String, Object> sendRequest(String type, Map<String, Object> data) throws MiniDBException {
        // Build request
        Map<String, Object> request = new LinkedHashMap<>();
        request.put("type", type);
        request.put("seq", nextSeq());
        request.put("data", data);
        
        String json = toJson(request);
        byte[] payload = json.getBytes(StandardCharsets.UTF_8);
        
        try {
            // Send header (6 bytes: 2 magic + 4 length)
            out.writeShort(PROTOCOL_MAGIC);
            out.writeInt(payload.length);
            out.write(payload);
            out.flush();
            
            // Receive header
            int magic = in.readUnsignedShort();
            int length = in.readInt();
            
            if (magic != PROTOCOL_MAGIC) {
                throw new MiniDBException("无效的协议标识: 0x" + Integer.toHexString(magic));
            }
            
            if (length > MAX_MESSAGE_SIZE) {
                throw new MiniDBException("响应过大: " + length);
            }
            
            // Receive payload
            byte[] respPayload = new byte[length];
            in.readFully(respPayload);
            
            String respJson = new String(respPayload, StandardCharsets.UTF_8);
            return parseJson(respJson);
            
        } catch (SocketTimeoutException e) {
            connected = false;
            throw new MiniDBException("请求超时", e);
        } catch (IOException e) {
            connected = false;
            throw new MiniDBException("通信错误: " + e.getMessage(), e);
        }
    }
    
    private QueryResult parseQueryResult(Map<String, Object> response) {
        @SuppressWarnings("unchecked")
        Map<String, Object> data = (Map<String, Object>) response.get("data");
        if (data == null) {
            data = new HashMap<>();
        }
        
        // Parse columns
        List<ColumnInfo> columns = new ArrayList<>();
        @SuppressWarnings("unchecked")
        List<Map<String, Object>> colList = (List<Map<String, Object>>) data.get("columns");
        if (colList != null) {
            for (Map<String, Object> col : colList) {
                columns.add(new ColumnInfo(
                    (String) col.get("name"),
                    (String) col.get("type")
                ));
            }
        }
        
        // Parse rows
        @SuppressWarnings("unchecked")
        List<List<Object>> rows = (List<List<Object>>) data.get("rows");
        if (rows == null) {
            rows = new ArrayList<>();
        }
        
        int rowCount = rows.size();
        Object rc = data.get("row_count");
        if (rc instanceof Number) {
            rowCount = ((Number) rc).intValue();
        }
        
        int affectedRows = 0;
        Object ar = data.get("affected_rows");
        if (ar instanceof Number) {
            affectedRows = ((Number) ar).intValue();
        }
        
        String message = (String) data.get("message");
        if (message == null) {
            message = "";
        }
        
        return new QueryResult(columns, rows, rowCount, affectedRows, message);
    }
    
    // Simple JSON serialization (no external dependencies)
    private String toJson(Map<String, Object> map) {
        StringBuilder sb = new StringBuilder();
        sb.append("{");
        boolean first = true;
        for (Map.Entry<String, Object> entry : map.entrySet()) {
            if (!first) sb.append(",");
            first = false;
            sb.append("\"").append(escape(entry.getKey())).append("\":");
            sb.append(toJsonValue(entry.getValue()));
        }
        sb.append("}");
        return sb.toString();
    }
    
    private String toJsonValue(Object value) {
        if (value == null) {
            return "null";
        } else if (value instanceof String) {
            return "\"" + escape((String) value) + "\"";
        } else if (value instanceof Number) {
            return value.toString();
        } else if (value instanceof Boolean) {
            return value.toString();
        } else if (value instanceof Map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> map = (Map<String, Object>) value;
            return toJson(map);
        } else if (value instanceof List) {
            StringBuilder sb = new StringBuilder();
            sb.append("[");
            boolean first = true;
            for (Object item : (List<?>) value) {
                if (!first) sb.append(",");
                first = false;
                sb.append(toJsonValue(item));
            }
            sb.append("]");
            return sb.toString();
        } else {
            return "\"" + escape(value.toString()) + "\"";
        }
    }
    
    private String escape(String s) {
        StringBuilder sb = new StringBuilder();
        for (char c : s.toCharArray()) {
            switch (c) {
                case '"': sb.append("\\\""); break;
                case '\\': sb.append("\\\\"); break;
                case '\n': sb.append("\\n"); break;
                case '\r': sb.append("\\r"); break;
                case '\t': sb.append("\\t"); break;
                default: sb.append(c); break;
            }
        }
        return sb.toString();
    }
    
    // Simple JSON parsing
    @SuppressWarnings("unchecked")
    private Map<String, Object> parseJson(String json) {
        return (Map<String, Object>) parseValue(new JsonReader(json));
    }
    
    private Object parseValue(JsonReader reader) {
        reader.skipWhitespace();
        char c = reader.peek();
        
        if (c == '{') {
            return parseObject(reader);
        } else if (c == '[') {
            return parseArray(reader);
        } else if (c == '"') {
            return parseString(reader);
        } else if (c == 't' || c == 'f') {
            return parseBoolean(reader);
        } else if (c == 'n') {
            return parseNull(reader);
        } else {
            return parseNumber(reader);
        }
    }
    
    private Map<String, Object> parseObject(JsonReader reader) {
        Map<String, Object> map = new LinkedHashMap<>();
        reader.expect('{');
        reader.skipWhitespace();
        
        if (reader.peek() != '}') {
            while (true) {
                reader.skipWhitespace();
                String key = parseString(reader);
                reader.skipWhitespace();
                reader.expect(':');
                Object value = parseValue(reader);
                map.put(key, value);
                
                reader.skipWhitespace();
                if (reader.peek() == ',') {
                    reader.next();
                } else {
                    break;
                }
            }
        }
        
        reader.expect('}');
        return map;
    }
    
    private List<Object> parseArray(JsonReader reader) {
        List<Object> list = new ArrayList<>();
        reader.expect('[');
        reader.skipWhitespace();
        
        if (reader.peek() != ']') {
            while (true) {
                Object value = parseValue(reader);
                list.add(value);
                
                reader.skipWhitespace();
                if (reader.peek() == ',') {
                    reader.next();
                } else {
                    break;
                }
            }
        }
        
        reader.expect(']');
        return list;
    }
    
    private String parseString(JsonReader reader) {
        reader.expect('"');
        StringBuilder sb = new StringBuilder();
        
        while (reader.peek() != '"') {
            char c = reader.next();
            if (c == '\\') {
                char escaped = reader.next();
                switch (escaped) {
                    case '"': sb.append('"'); break;
                    case '\\': sb.append('\\'); break;
                    case 'n': sb.append('\n'); break;
                    case 'r': sb.append('\r'); break;
                    case 't': sb.append('\t'); break;
                    default: sb.append(escaped); break;
                }
            } else {
                sb.append(c);
            }
        }
        
        reader.expect('"');
        return sb.toString();
    }
    
    private Number parseNumber(JsonReader reader) {
        StringBuilder sb = new StringBuilder();
        while (!reader.eof() && "0123456789.-+eE".indexOf(reader.peek()) >= 0) {
            sb.append(reader.next());
        }
        String num = sb.toString();
        if (num.contains(".") || num.contains("e") || num.contains("E")) {
            return Double.parseDouble(num);
        } else {
            return Long.parseLong(num);
        }
    }
    
    private Boolean parseBoolean(JsonReader reader) {
        if (reader.peek() == 't') {
            reader.expect('t');
            reader.expect('r');
            reader.expect('u');
            reader.expect('e');
            return true;
        } else {
            reader.expect('f');
            reader.expect('a');
            reader.expect('l');
            reader.expect('s');
            reader.expect('e');
            return false;
        }
    }
    
    private Object parseNull(JsonReader reader) {
        reader.expect('n');
        reader.expect('u');
        reader.expect('l');
        reader.expect('l');
        return null;
    }
    
    private static class JsonReader {
        private final String json;
        private int pos = 0;
        
        JsonReader(String json) {
            this.json = json;
        }
        
        char peek() {
            return json.charAt(pos);
        }
        
        char next() {
            return json.charAt(pos++);
        }
        
        void expect(char c) {
            if (next() != c) {
                throw new RuntimeException("Expected '" + c + "' at position " + (pos - 1));
            }
        }
        
        boolean eof() {
            return pos >= json.length();
        }
        
        void skipWhitespace() {
            while (!eof() && Character.isWhitespace(peek())) {
                pos++;
            }
        }
    }
    
    // Builder pattern for advanced configuration
    public static Builder builder() {
        return new Builder();
    }
    
    public static class Builder {
        private String host = "127.0.0.1";
        private int port = DEFAULT_PORT;
        private String username = "";
        private String password = "";
        private int timeout = DEFAULT_TIMEOUT;
        
        public Builder host(String host) { this.host = host; return this; }
        public Builder port(int port) { this.port = port; return this; }
        public Builder username(String username) { this.username = username; return this; }
        public Builder password(String password) { this.password = password; return this; }
        public Builder timeout(int timeout) { this.timeout = timeout; return this; }
        
        public MiniDBClient build() throws MiniDBException {
            return new MiniDBClient(host, port, username, password, timeout);
        }
    }
}
