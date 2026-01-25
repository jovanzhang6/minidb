package com.minidb;

/**
 * MiniDB Java Client Demo Test
 * 
 * Usage:
 *   java com.minidb.DemoTest [host] [port] [user] [password]
 * 
 * Default:
 *   host: 127.0.0.1
 *   port: 9527
 *   user: root
 *   password: 123456
 */
public class DemoTest {
    
    private static void printSeparator(String title) {
        System.out.println();
        System.out.println("============================================================");
        if (title != null && !title.isEmpty()) {
            System.out.println("  " + title);
            System.out.println("============================================================");
        }
    }
    
    private static void printResult(QueryResult result) {
        if (result.getColumns() != null && !result.getColumns().isEmpty()) {
            // Print column headers
            StringBuilder header = new StringBuilder("| ");
            for (ColumnInfo col : result.getColumns()) {
                header.append(String.format("%15s | ", col.getName()));
            }
            System.out.println(header);
            
            // Print separator line
            StringBuilder sep = new StringBuilder("|");
            for (int i = 0; i < result.getColumns().size(); i++) {
                sep.append("-----------------");
            }
            sep.append("|");
            System.out.println(sep);
            
            // Print rows
            for (int i = 0; i < result.getRowCount(); i++) {
                StringBuilder row = new StringBuilder("| ");
                for (Object val : result.getRow(i)) {
                    String strVal = val != null ? val.toString() : "NULL";
                    row.append(String.format("%15s | ", strVal));
                }
                System.out.println(row);
            }
            
            System.out.println("\n(" + result.getRowCount() + " rows)");
        } else {
            if (result.getAffectedRows() > 0) {
                System.out.println("Affected rows: " + result.getAffectedRows());
            }
            if (result.getMessage() != null && !result.getMessage().isEmpty()) {
                System.out.println("Message: " + result.getMessage());
            }
        }
    }
    
    public static void main(String[] args) {
        // Parse command line arguments
        String host = args.length > 0 ? args[0] : "127.0.0.1";
        int port = args.length > 1 ? Integer.parseInt(args[1]) : 9527;
        String user = args.length > 2 ? args[2] : "root";
        String password = args.length > 3 ? args[3] : "123456";
        
        printSeparator("MiniDB Java Client Demo");
        System.out.println("Connecting to " + host + ":" + port + " as '" + user + "'...");
        
        MiniDBClient client = null;
        
        try {
            // Connect to server
            client = new MiniDBClient(host, port, user, password);
            System.out.println("Connected successfully!");
            System.out.println("Session ID: " + client.getSessionId());
            
            // Test 1: Simple expression
            printSeparator("Test 1: Simple Expression");
            System.out.println("SQL: SELECT 1 + 1 AS result");
            QueryResult result = client.execute("SELECT 1 + 1 AS result");
            printResult(result);
            
            // Test 2: Create table
            printSeparator("Test 2: Create Table");
            String createSql = 
                "CREATE TABLE IF NOT EXISTS demo_products (" +
                "    id INTEGER PRIMARY KEY," +
                "    name TEXT NOT NULL," +
                "    price REAL," +
                "    quantity INTEGER" +
                ")";
            System.out.println("SQL: " + createSql);
            result = client.execute(createSql);
            System.out.println("Table created/verified successfully!");
            
            // Test 3: Insert data
            printSeparator("Test 3: Insert Data");
            String[] inserts = {
                "INSERT INTO demo_products VALUES (1, 'Apple', 5.5, 100)",
                "INSERT INTO demo_products VALUES (2, 'Banana', 3.0, 150)",
                "INSERT INTO demo_products VALUES (3, 'Orange', 4.5, 80)",
                "INSERT INTO demo_products VALUES (4, 'Grape', 8.0, 60)",
                "INSERT INTO demo_products VALUES (5, 'Mango', 6.5, 45)"
            };
            for (String sql : inserts) {
                System.out.println("SQL: " + sql);
                try {
                    result = client.execute(sql);
                    System.out.println("  -> Inserted successfully");
                } catch (QueryException e) {
                    System.out.println("  -> Skipped (may already exist): " + e.getMessage());
                }
            }
            
            // Test 4: Select all
            printSeparator("Test 4: Select All");
            System.out.println("SQL: SELECT * FROM demo_products");
            result = client.execute("SELECT * FROM demo_products");
            printResult(result);
            
            // Test 5: Select with condition
            printSeparator("Test 5: Select with WHERE");
            System.out.println("SQL: SELECT * FROM demo_products WHERE price > 5");
            result = client.execute("SELECT * FROM demo_products WHERE price > 5");
            printResult(result);
            
            // Test 6: Select with ORDER BY
            printSeparator("Test 6: Select with ORDER BY");
            System.out.println("SQL: SELECT name, price FROM demo_products ORDER BY price DESC");
            result = client.execute("SELECT name, price FROM demo_products ORDER BY price DESC");
            printResult(result);
            
            // Test 7: Aggregate function
            printSeparator("Test 7: Aggregate Functions");
            System.out.println("SQL: SELECT COUNT(*) as cnt, SUM(quantity) as total FROM demo_products");
            result = client.execute("SELECT COUNT(*) as cnt, SUM(quantity) as total FROM demo_products");
            printResult(result);
            
            // Test 8: Update
            printSeparator("Test 8: Update");
            System.out.println("SQL: UPDATE demo_products SET price = 6.0 WHERE name = 'Apple'");
            result = client.execute("UPDATE demo_products SET price = 6.0 WHERE name = 'Apple'");
            System.out.println("Updated " + result.getAffectedRows() + " row(s)");
            
            // Verify update
            System.out.println("\nSQL: SELECT * FROM demo_products WHERE name = 'Apple'");
            result = client.execute("SELECT * FROM demo_products WHERE name = 'Apple'");
            printResult(result);
            
            // Test 9: Delete
            printSeparator("Test 9: Delete");
            System.out.println("SQL: DELETE FROM demo_products WHERE id = 5");
            result = client.execute("DELETE FROM demo_products WHERE id = 5");
            System.out.println("Deleted " + result.getAffectedRows() + " row(s)");
            
            // Verify delete
            System.out.println("\nSQL: SELECT * FROM demo_products");
            result = client.execute("SELECT * FROM demo_products");
            printResult(result);
            
            // Test 10: Ping
            printSeparator("Test 10: Ping");
            if (client.ping()) {
                System.out.println("Ping successful - server is responsive");
            } else {
                System.out.println("Ping failed!");
            }
            
            // Cleanup
            printSeparator("Cleanup");
            System.out.println("SQL: DROP TABLE demo_products");
            try {
                result = client.execute("DROP TABLE demo_products");
                System.out.println("Table dropped successfully!");
            } catch (QueryException e) {
                System.out.println("Could not drop table: " + e.getMessage());
            }
            
            printSeparator("All Tests Completed!");
            
        } catch (AuthenticationException e) {
            System.err.println("Authentication failed: " + e.getMessage());
            System.exit(1);
        } catch (MiniDBException e) {
            System.err.println("Database error: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        } finally {
            System.out.println("\nClosing connection...");
            if (client != null) {
                client.close();
            }
            System.out.println("Disconnected.");
        }
    }
}
