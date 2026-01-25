#!/usr/bin/env python3
"""
MiniDB Python Client Demo Test

Usage:
    python demo_test.py [--host HOST] [--port PORT] [--user USER] [--password PASS]
    
Default:
    --host 127.0.0.1
    --port 9527
    --user root
    --password 123456
"""

import sys
import argparse
from minidb_client import MiniDBClient, MiniDBError, AuthenticationError, QueryError


def print_separator(title=""):
    print("\n" + "=" * 60)
    if title:
        print(f"  {title}")
        print("=" * 60)


def print_result(result):
    """Pretty print query result"""
    if result.columns:
        # Print column headers
        col_names = [col.name for col in result.columns]
        print("| " + " | ".join(f"{name:>15}" for name in col_names) + " |")
        print("|" + "-" * (17 * len(col_names) + len(col_names) - 1) + "|")
        
        # Print rows
        for row in result.rows:
            formatted = [str(v) if v is not None else "NULL" for v in row]
            print("| " + " | ".join(f"{v:>15}" for v in formatted) + " |")
        
        print(f"\n({result.row_count} rows)")
    else:
        if result.affected_rows > 0:
            print(f"Affected rows: {result.affected_rows}")
        if result.message:
            print(f"Message: {result.message}")


def run_demo(host, port, username, password):
    """Run the demo test"""
    
    print_separator("MiniDB Python Client Demo")
    print(f"Connecting to {host}:{port} as '{username}'...")
    
    try:
        # Connect to server
        client = MiniDBClient(host, port, username, password)
        print("Connected successfully!")
        print(f"Session ID: {client.session_id}")
        
    except AuthenticationError as e:
        print(f"Authentication failed: {e}")
        return 1
    except MiniDBError as e:
        print(f"Connection failed: {e}")
        return 1
    
    try:
        # Test 1: Simple expression
        print_separator("Test 1: Simple Expression")
        print("SQL: SELECT 1 + 1 AS result")
        result = client.execute("SELECT 1 + 1 AS result")
        print_result(result)
        
        # Test 2: Create table
        print_separator("Test 2: Create Table")
        create_sql = """
        CREATE TABLE IF NOT EXISTS demo_students (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            age INTEGER,
            score REAL
        )
        """
        print(f"SQL: {create_sql.strip()}")
        result = client.execute(create_sql)
        print("Table created/verified successfully!")
        
        # Test 3: Insert data
        print_separator("Test 3: Insert Data")
        inserts = [
            "INSERT INTO demo_students VALUES (1, 'Alice', 20, 95.5)",
            "INSERT INTO demo_students VALUES (2, 'Bob', 21, 88.0)",
            "INSERT INTO demo_students VALUES (3, 'Charlie', 19, 92.5)",
            "INSERT INTO demo_students VALUES (4, 'Diana', 22, 91.0)",
            "INSERT INTO demo_students VALUES (5, 'Eve', 20, 87.5)",
        ]
        for sql in inserts:
            print(f"SQL: {sql}")
            try:
                result = client.execute(sql)
                print(f"  -> Inserted successfully")
            except QueryError as e:
                print(f"  -> Skipped (may already exist): {e}")
        
        # Test 4: Select all
        print_separator("Test 4: Select All")
        print("SQL: SELECT * FROM demo_students")
        result = client.execute("SELECT * FROM demo_students")
        print_result(result)
        
        # Test 5: Select with condition
        print_separator("Test 5: Select with WHERE")
        print("SQL: SELECT * FROM demo_students WHERE age >= 20")
        result = client.execute("SELECT * FROM demo_students WHERE age >= 20")
        print_result(result)
        
        # Test 6: Select with ORDER BY
        print_separator("Test 6: Select with ORDER BY")
        print("SQL: SELECT name, score FROM demo_students ORDER BY score DESC")
        result = client.execute("SELECT name, score FROM demo_students ORDER BY score DESC")
        print_result(result)
        
        # Test 7: Aggregate function
        print_separator("Test 7: Aggregate Functions")
        print("SQL: SELECT COUNT(*) as cnt, AVG(score) as avg_score FROM demo_students")
        result = client.execute("SELECT COUNT(*) as cnt, AVG(score) as avg_score FROM demo_students")
        print_result(result)
        
        # Test 8: Update
        print_separator("Test 8: Update")
        print("SQL: UPDATE demo_students SET score = 96.0 WHERE name = 'Alice'")
        result = client.execute("UPDATE demo_students SET score = 96.0 WHERE name = 'Alice'")
        print(f"Updated {result.affected_rows} row(s)")
        
        # Verify update
        print("\nSQL: SELECT * FROM demo_students WHERE name = 'Alice'")
        result = client.execute("SELECT * FROM demo_students WHERE name = 'Alice'")
        print_result(result)
        
        # Test 9: Delete
        print_separator("Test 9: Delete")
        print("SQL: DELETE FROM demo_students WHERE id = 5")
        result = client.execute("DELETE FROM demo_students WHERE id = 5")
        print(f"Deleted {result.affected_rows} row(s)")
        
        # Verify delete
        print("\nSQL: SELECT * FROM demo_students")
        result = client.execute("SELECT * FROM demo_students")
        print_result(result)
        
        # Test 10: Ping
        print_separator("Test 10: Ping")
        if client.ping():
            print("Ping successful - server is responsive")
        else:
            print("Ping failed!")
        
        # Cleanup (optional)
        print_separator("Cleanup")
        print("SQL: DROP TABLE demo_students")
        try:
            result = client.execute("DROP TABLE demo_students")
            print("Table dropped successfully!")
        except QueryError as e:
            print(f"Could not drop table: {e}")
        
        print_separator("All Tests Completed!")
        return 0
        
    except QueryError as e:
        print(f"\nQuery error: {e}")
        return 1
    except MiniDBError as e:
        print(f"\nDatabase error: {e}")
        return 1
    finally:
        print("\nClosing connection...")
        client.close()
        print("Disconnected.")


def main():
    parser = argparse.ArgumentParser(description='MiniDB Python Client Demo Test')
    parser.add_argument('--host', default='127.0.0.1', help='Server host (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=9527, help='Server port (default: 9527)')
    parser.add_argument('--user', default='root', help='Username (default: root)')
    parser.add_argument('--password', default='123456', help='Password (default: 123456)')
    
    args = parser.parse_args()
    
    return run_demo(args.host, args.port, args.user, args.password)


if __name__ == "__main__":
    sys.exit(main())
