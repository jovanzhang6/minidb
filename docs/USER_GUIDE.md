# MiniDB User Guide

## Introduction
MiniDB is a lightweight, SQLite-style relational database management system. This guide covers how to build, run, and use the MiniDB CLI shell.

## 1. Building MiniDB

MiniDB uses CMake. Ensure you have CMake (3.10+) and a C++17 compatible compiler.

### Linux/macOS
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Windows (Visual Studio)
```powershell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

## 2. Running the Shell

The main executable is `minidb` (or `minidb.exe` on Windows).

```bash
./minidb [database_file]
```
If `database_file` is provided, it will be opened. Otherwise, you can open one inside the shell.

## 3. Shell Commands

Once inside the shell (`minidb> `), you can use meta-commands (starting with `.`) or execute SQL statements (ending with `;`).

### Meta-commands
- `.help`: Show help message.
- `.open FILENAME`: Open (or create) a database file.
- `.close`: Close the current database.
- `.tables`: List all tables in the current database.
- `.schema`: Show schema information (TODO).
- `.quit` or `.exit`: Exit the shell.

### SQL Features
MiniDB supports standard SQL syntax for DDL, DML, and TCL.

#### Data Definition Language (DDL)
```sql
CREATE TABLE users (id INT, name TEXT, score FLOAT);
DROP TABLE users;
```

#### Data Manipulation Language (DML)
```sql
INSERT INTO users VALUES (1, 'Alice', 95.5);
INSERT INTO users VALUES (2, 'Bob', 80.0);
UPDATE users SET score = 100.0 WHERE id = 1;
DELETE FROM users WHERE score < 90;
SELECT * FROM users WHERE id = 1;
SELECT name, score FROM users;
```

#### Transaction Control Language (TCL)
```sql
BEGIN;
INSERT INTO users VALUES (3, 'Charlie', 70);
ROLLBACK; -- Discards changes

BEGIN;
INSERT INTO users VALUES (4, 'Dave', 88);
COMMIT; -- Saves changes
```

## 4. Limitations
- Single active transaction per session.
- No network support (embedded/file-based only).
- Limited data types (INT, FLOAT, TEXT).
