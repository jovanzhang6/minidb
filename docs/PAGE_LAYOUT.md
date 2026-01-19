# MiniDB 页面布局设计文档

本文档详细定义了 MiniDB 中各类页面的字节布局，是数据持久化的核心规范。

---

## 1. 概述

### 1.1 基本参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 页面大小 | 4096 字节 | 固定不变 |
| 文件头大小 | 100 字节 | 位于 Page 0 开头 |
| 最大 varint 长度 | 9 字节 | 可编码 64 位整数 |

### 1.2 页面类型

| 类型值 | 名称 | 说明 |
|--------|------|------|
| 0x00 | PAGE_INVALID | 无效页面 |
| 0x01 | PAGE_FREELIST_TRUNK | 空闲页链表主干页 |
| 0x02 | INDEX_INTERIOR | 索引 B-tree 内部页 |
| 0x03 | PAGE_OVERFLOW | 溢出页 |
| 0x05 | TABLE_INTERIOR | 表 B-tree 内部页 |
| 0x0a | INDEX_LEAF | 索引 B-tree 叶子页 |
| 0x0d | TABLE_LEAF | 表 B-tree 叶子页 |

---

## 2. 数据库文件整体布局

### 2.1 文件结构概览

```
┌─────────────────────────────────────────┐  Page 0 (偏移 0)
│         数据库文件头 (100 字节)          │
│         + 页面剩余空间                   │
├─────────────────────────────────────────┤  Page 1 (偏移 4096)
│         sys_tables B-tree 根页          │
│         (表元数据)                       │
├─────────────────────────────────────────┤  Page 2 (偏移 8192)
│         sys_columns B-tree 根页         │
│         (列定义元数据)                   │
├─────────────────────────────────────────┤  Page 3 (偏移 12288)
│         sys_users B-tree 根页           │
│         (用户信息)                       │
├─────────────────────────────────────────┤  Page 4 (偏移 16384)
│         sys_privileges B-tree 根页      │
│         (权限信息)                       │
├─────────────────────────────────────────┤  Page 5 (偏移 20480)
│         sys_indexes B-tree 根页         │
│         (索引元数据)                     │
├─────────────────────────────────────────┤  Page 6 (偏移 24576)
│         sys_views B-tree 根页           │
│         (视图元数据)                     │
├─────────────────────────────────────────┤  Page 7+ (偏移 28672+)
│         用户表数据页 / 索引页 / 溢出页    │
│         (动态分配)                       │
└─────────────────────────────────────────┘
```

### 2.2 Page 0: 数据库文件头

文件头占据 Page 0 的前 100 字节，存储数据库全局元信息。

```
偏移    大小    字段                描述
────────────────────────────────────────────────────────────────
0       16      magic               魔数 "MiniDB format 1\0"
16      2       page_size           页大小，固定 4096 (0x1000)
18      4       page_count          数据库文件总页数
22      4       first_free_page     空闲页链表头页号，0表示无
26      4       free_page_count     空闲页总数
30      4       schema_version      Schema版本，DDL操作时递增
34      4       user_version        用户自定义版本号
38      8       next_rowid          下一个可用的全局rowid
46      54      reserved            保留字段，填充0
────────────────────────────────────────────────────────────────
总计: 100 字节
```

**魔数 (Magic Number):**
```
字节: 4D 69 6E 69 44 42 20 66 6F 72 6D 61 74 20 31 00
文本: M  i  n  i  D  B     f  o  r  m  a  t     1  \0
```

**验证逻辑:** 打开数据库文件时，检查前 15 字节是否匹配魔数，以及 page_size 是否为 4096。

---

## 3. 系统表详细结构

系统表使用与用户表相同的 B-tree 存储结构，rowid 自动生成。

### 3.1 sys_tables (Page 1)

存储所有用户表的元数据。

**Schema:**
| 列序号 | 列名 | 类型 | 说明 |
|--------|------|------|------|
| 0 | table_id | INT | 表唯一标识符 |
| 1 | table_name | TEXT | 表名（唯一） |
| 2 | root_page | INT | 该表 B-tree 根页 ID |
| 3 | next_rowid | INT | 该表下一个可用 rowid |

**记录示例:**
```
rowid=1: (1, "students", 7, 100)
         表ID=1, 名称=students, 根页=7, 下一个rowid=100
```

### 3.2 sys_columns (Page 2)

存储所有表的列定义。

**Schema:**
| 列序号 | 列名 | 类型 | 说明 |
|--------|------|------|------|
| 0 | table_id | INT | 所属表ID（外键） |
| 1 | column_id | INT | 列在表内的序号（0起始） |
| 2 | column_name | TEXT | 列名 |
| 3 | data_type | INT | 数据类型枚举值 |
| 4 | nullable | INT | 是否可空（1=是，0=否） |
| 5 | is_primary_key | INT | 是否主键（1=是，0=否） |

**数据类型枚举:**
| 值 | 类型 |
|----|------|
| 0 | INVALID |
| 1 | INT |
| 2 | FLOAT |
| 3 | TEXT |

**记录示例:**
```
rowid=1: (1, 0, "id", 1, 0, 1)    -- students.id: INT NOT NULL PRIMARY KEY
rowid=2: (1, 1, "name", 3, 1, 0) -- students.name: TEXT NULL
rowid=3: (1, 2, "age", 1, 1, 0)  -- students.age: INT NULL
```

### 3.3 sys_users (Page 3)

存储用户账号信息。

**Schema:**
| 列序号 | 列名 | 类型 | 说明 |
|--------|------|------|------|
| 0 | user_id | INT | 用户唯一标识符 |
| 1 | username | TEXT | 用户名（唯一） |
| 2 | password_hash | TEXT | 密码哈希值 |
| 3 | is_admin | INT | 是否管理员（1=是，0=否） |

**注意:** 初始化时自动创建 root 用户（密码 123456），is_admin=1。

**记录示例:**
```
rowid=1: (1, "root", "e10adc...", 1)  -- 管理员
rowid=2: (2, "alice", "5f4dcc...", 0) -- 普通用户
```

### 3.4 sys_privileges (Page 4)

存储用户权限信息。

**Schema:**
| 列序号 | 列名 | 类型 | 说明 |
|--------|------|------|------|
| 0 | user_id | INT | 用户ID（外键） |
| 1 | table_id | INT | 表ID（外键，0表示所有表） |
| 2 | privilege_type | INT | 权限类型枚举值 |

**权限类型枚举:**
| 值 | 权限 |
|----|------|
| 1 | SELECT |
| 2 | INSERT |
| 3 | UPDATE |
| 4 | DELETE |
| 99 | ALL |

**记录示例:**
```
rowid=1: (2, 1, 1)  -- alice 对 table_id=1 有 SELECT 权限
rowid=2: (2, 1, 2)  -- alice 对 table_id=1 有 INSERT 权限
```

### 3.5 sys_indexes (Page 5)

存储索引元数据。

**Schema:**
| 列序号 | 列名 | 类型 | 说明 |
|--------|------|------|------|
| 0 | index_id | INT | 索引唯一标识符 |
| 1 | index_name | TEXT | 索引名（唯一） |
| 2 | table_id | INT | 所属表ID（外键） |
| 3 | column_id | INT | 索引列序号 |
| 4 | root_page | INT | 索引 B-tree 根页 ID |
| 5 | is_unique | INT | 是否唯一索引（1=是，0=否） |

**记录示例:**
```
rowid=1: (1, "idx_students_name", 1, 1, 10, 0)
         -- students 表 name 列上的非唯一索引，根页=10
```

### 3.6 sys_views (Page 6)

存储视图定义。

**Schema:**
| 列序号 | 列名 | 类型 | 说明 |
|--------|------|------|------|
| 0 | view_id | INT | 视图唯一标识符 |
| 1 | view_name | TEXT | 视图名（唯一） |
| 2 | view_definition | TEXT | 视图的 SELECT SQL 语句 |

**记录示例:**
```
rowid=1: (1, "senior_employees", "SELECT * FROM employees WHERE age >= 25")
```

---

## 4. B-tree 页面结构

### 4.1 B-tree 页面通用布局

```
┌─────────────────────────────────────────┐  偏移 0 (或100如果是Page 0)
│            B-tree 页头                  │
│         (8字节叶子页 / 12字节内部页)     │
├─────────────────────────────────────────┤
│         Cell 指针数组                    │
│    (每个指针2字节，从低地址向高增长)      │
├─────────────────────────────────────────┤
│                                         │
│            未分配空间                    │
│           (可用于新Cell)                 │
├─────────────────────────────────────────┤
│         Cell 内容区域                    │
│      (从高地址向低地址增长)              │
└─────────────────────────────────────────┘  偏移 4095
```

**空间增长方向:**
- Cell 指针数组：从页头后向高地址增长（↓）
- Cell 内容区域：从页尾向低地址增长（↑）
- 两者相遇时页面满

### 4.2 叶子页页头（8字节）

```
偏移    大小    字段                描述
────────────────────────────────────────────────────────────
0       1       page_type           页类型 (0x0a 索引叶子 或 0x0d 表叶子)
1       2       first_freeblock     第一个空闲块偏移，0表示无
3       2       cell_count          页内Cell数量
5       2       cell_content_start  Cell内容区起始偏移，0表示4096
7       1       fragmented_bytes    碎片字节数
────────────────────────────────────────────────────────────
总计: 8 字节
```

### 4.3 内部页页头（12字节）

```
偏移    大小    字段                描述
────────────────────────────────────────────────────────────
0       1       page_type           页类型 (0x02 索引内部 或 0x05 表内部)
1       2       first_freeblock     第一个空闲块偏移，0表示无
3       2       cell_count          页内Cell数量
5       2       cell_content_start  Cell内容区起始偏移，0表示4096
7       1       fragmented_bytes    碎片字节数
8       4       right_child         最右子页页号
────────────────────────────────────────────────────────────
总计: 12 字节
```

### 4.4 Cell 指针数组

位于页头之后，每个指针 2 字节，存储对应 Cell 在页内的偏移量。

```
页头后立即开始:
┌────────┬────────┬────────┬────────┬─────┐
│ ptr[0] │ ptr[1] │ ptr[2] │ ptr[3] │ ... │
│ 2bytes │ 2bytes │ 2bytes │ 2bytes │     │
└────────┴────────┴────────┴────────┴─────┘

ptr[i] = Cell[i] 在页内的字节偏移量
```

**排序:** Cell 指针按键值排序存储，便于二分查找。

---

## 5. 表页面 Cell 格式

### 5.1 Table Leaf Cell (表叶子页, 0x0d)

存储实际的行数据。

```
┌─────────────────────────────────────────┐
│  payload_size (varint)                  │  负载总大小
├─────────────────────────────────────────┤
│  rowid (varint)                         │  行ID（主键）
├─────────────────────────────────────────┤
│  payload (bytes)                        │  记录内容（见下方）
├─────────────────────────────────────────┤
│  overflow_page (4 bytes, 可选)          │  溢出页号
└─────────────────────────────────────────┘
```

**Cell 大小计算:**
```cpp
cell_size = varint_size(payload_size) + varint_size(rowid) + payload_size
            + (有溢出 ? 4 : 0)
```

### 5.2 Table Interior Cell (表内部页, 0x05)

用于 B-tree 导航。

```
┌─────────────────────────────────────────┐
│  left_child (4 bytes)                   │  左子页页号
├─────────────────────────────────────────┤
│  rowid (varint)                         │  分隔键（rowid）
└─────────────────────────────────────────┘
```

**导航规则:**
- rowid < cell[i].rowid → 进入 cell[i].left_child
- rowid ≥ cell[i].rowid → 进入下一个 cell 或 right_child

---

## 6. 索引页面 Cell 格式

### 6.1 Index Leaf Cell (索引叶子页, 0x0a)

存储索引键和对应的 rowid。

```
┌─────────────────────────────────────────┐
│  key_size (varint)                      │  键序列化大小
├─────────────────────────────────────────┤
│  key_data (bytes)                       │  序列化的键值
├─────────────────────────────────────────┤
│  rowid (varint)                         │  指向数据行的 rowid
└─────────────────────────────────────────┘
```

**排序:** 按 (key_value, rowid) 复合排序，支持重复键。

### 6.2 Index Interior Cell (索引内部页, 0x02)

```
┌─────────────────────────────────────────┐
│  left_child (4 bytes)                   │  左子页页号
├─────────────────────────────────────────┤
│  key_size (varint)                      │  键序列化大小
├─────────────────────────────────────────┤
│  key_data (bytes)                       │  序列化的分隔键
└─────────────────────────────────────────┘
```

---

## 7. 记录 (Record) 序列化格式

记录的 payload 部分采用类似 SQLite 的格式。

### 7.1 记录布局

```
┌─────────────────────────────────────────┐
│  header_size (varint)                   │  头部总大小（含自身）
├─────────────────────────────────────────┤
│  serial_type_1 (varint)                 │  列1的类型编码
│  serial_type_2 (varint)                 │  列2的类型编码
│  ...                                    │
├─────────────────────────────────────────┤
│  value_1 (bytes)                        │  列1的值
│  value_2 (bytes)                        │  列2的值
│  ...                                    │
└─────────────────────────────────────────┘
```

### 7.2 Serial Type 编码

| Serial Type | 含义 | 值大小 |
|-------------|------|--------|
| 0 | NULL | 0 字节 |
| 1 | 8-bit 有符号整数 | 1 字节 |
| 2 | 16-bit 大端整数 | 2 字节 |
| 3 | 24-bit 大端整数 | 3 字节 |
| 4 | 32-bit 大端整数 | 4 字节 |
| 5 | 48-bit 大端整数 | 6 字节 |
| 6 | 64-bit 大端整数 | 8 字节 |
| 7 | IEEE 754 双精度浮点 | 8 字节 |
| 8 | 整数常量 0 | 0 字节 |
| 9 | 整数常量 1 | 0 字节 |
| N≥12 且偶数 | BLOB，大小=(N-12)/2 | (N-12)/2 字节 |
| N≥13 且奇数 | TEXT，大小=(N-13)/2 | (N-13)/2 字节 |

**TEXT 示例:**
- 空字符串 "" → serial_type = 13, 值大小 = 0
- "hello" (5字节) → serial_type = 13 + 5*2 = 23, 值大小 = 5

### 7.3 记录序列化示例

记录: `(1, "Alice", 20.5)`

```
Header:
  header_size = 4 (varint编码)
  serial_type[0] = 1 (8-bit INT for value 1)
  serial_type[1] = 23 (TEXT, len=5)
  serial_type[2] = 7 (FLOAT64)

Values:
  value[0] = 0x01 (1字节)
  value[1] = "Alice" (5字节)
  value[2] = 0x4034800000000000 (8字节, IEEE 754)

总计: 4 + 1 + 5 + 8 = 18 字节
```

---

## 8. Varint 变长整数编码

### 8.1 编码规则

- 每字节最高位 (bit 7) 为继续标志：1=后续有更多字节，0=结束
- 每字节低 7 位为数据位
- 大端序排列
- 最多 9 字节，可编码 64 位整数

### 8.2 编码示例

| 值 | 字节数 | 编码 |
|----|--------|------|
| 0 | 1 | `0x00` |
| 1 | 1 | `0x01` |
| 127 | 1 | `0x7F` |
| 128 | 2 | `0x81 0x00` |
| 255 | 2 | `0x81 0x7F` |
| 16383 | 2 | `0xFF 0x7F` |
| 16384 | 3 | `0x81 0x80 0x00` |

### 8.3 编码伪代码

```cpp
int Varint::Encode(uint64_t value, uint8_t* buffer) {
    int len = 0;
    uint8_t temp[9];
    do {
        temp[len++] = value & 0x7F;
        value >>= 7;
    } while (value > 0);
    
    for (int i = len - 1; i >= 0; i--) {
        *buffer++ = temp[i] | (i > 0 ? 0x80 : 0x00);
    }
    return len;
}
```

---

## 9. 空间管理

### 9.1 空闲块 (Freeblock)

页内删除 Cell 后的空间以链表管理。

```
┌─────────────────────────────────────────┐
│  next_freeblock (2 bytes)               │  下一个空闲块偏移，0=末尾
├─────────────────────────────────────────┤
│  size (2 bytes)                         │  本块大小（含头部4字节）
├─────────────────────────────────────────┤
│  free space (size - 4 bytes)            │  可用空间
└─────────────────────────────────────────┘
```

**最小空闲块:** 4 字节（仅头部）

### 9.2 碎片管理

- 小于 4 字节的空间无法形成 freeblock，计入 `fragmented_bytes`
- 当 `fragmented_bytes > 60` 时触发页面整理 (defragment)
- 整理后所有 Cell 紧凑排列，freeblock 链表清空

### 9.3 空闲页链表 (Freelist)

整个页面被释放时加入空闲页链表。

**Freelist Trunk Page 格式:**
```
┌─────────────────────────────────────────┐  偏移 0
│  next_trunk (4 bytes)                   │  下一个trunk页号，0=末尾
├─────────────────────────────────────────┤  偏移 4
│  leaf_count (4 bytes)                   │  本trunk管理的叶子页数
├─────────────────────────────────────────┤  偏移 8
│  leaf_pages[0..N] (各4字节)             │  空闲页号数组
└─────────────────────────────────────────┘

每个 trunk 页最多存储 (4096-8)/4 = 1022 个页号
```

---

## 10. 溢出页

当 Cell payload 超过阈值时，超出部分存储在溢出页链表。

**溢出阈值:** `4096 - 35 = 4061` 字节

**溢出页格式:**
```
┌─────────────────────────────────────────┐  偏移 0
│  next_overflow (4 bytes)                │  下一个溢出页号，0=末尾
├─────────────────────────────────────────┤  偏移 4
│  payload_data (4092 bytes)              │  溢出数据
└─────────────────────────────────────────┘
```

---

## 11. 页面操作流程

### 11.1 插入 Cell

1. 计算 Cell 所需大小
2. 检查 `未分配空间 >= Cell大小 + 2` (含指针)
3. 若空间不足，尝试从 freeblock 分配
4. 若仍不足且 `fragmented_bytes > 0`，执行整理
5. 从 `cell_content_start` 向下分配空间
6. 写入 Cell 数据
7. 在指针数组插入新指针（二分定位，保持有序）
8. 更新 `cell_count` 和 `cell_content_start`

### 11.2 删除 Cell

1. 根据索引获取 Cell 指针
2. 计算 Cell 大小
3. 将空间转为 freeblock 加入链表
4. 从指针数组删除对应项
5. 更新 `cell_count`

### 11.3 页面整理 (Defragment)

1. 收集所有有效 Cell（按指针数组顺序）
2. 从页尾重新紧凑排列
3. 更新所有 Cell 指针
4. 清空 freeblock 链表
5. 重置 `fragmented_bytes = 0`

---

## 12. 设计约束与限制

| 项目 | 限制 |
|------|------|
| 页面大小 | 固定 4096 字节 |
| 单条记录最大 | 约 4000 字节（不使用溢出） |
| 表名/列名最大 | 无硬性限制，受页面大小约束 |
| 每表最大列数 | 无硬性限制 |
| rowid 范围 | 64 位有符号整数 |
| 索引键类型 | INT, FLOAT, TEXT |
| 系统表数量 | 6 个（固定 Page 1-6） |

---

## 附录 A: C++ 结构体定义

```cpp
// 数据库文件头 (100 字节)
#pragma pack(push, 1)
struct DatabaseHeader {
    char magic[16];           // "MiniDB format 1\0"
    uint16_t page_size;       // 4096
    uint32_t page_count;      // 总页数
    uint32_t first_free_page; // 空闲页链表头
    uint32_t free_page_count; // 空闲页数
    uint32_t schema_version;  // Schema 版本
    uint32_t user_version;    // 用户版本
    uint64_t next_rowid;      // 下一个 rowid
    char reserved[54];        // 保留
};
#pragma pack(pop)

// B-tree 页头
#pragma pack(push, 1)
struct BTreePageHeader {
    uint8_t page_type;
    uint16_t first_freeblock;
    uint16_t cell_count;
    uint16_t cell_content_start;
    uint8_t fragmented_bytes;
    // 内部页额外字段:
    // uint32_t right_child;
};
#pragma pack(pop)

// 空闲块
#pragma pack(push, 1)
struct Freeblock {
    uint16_t next_offset;
    uint16_t size;
};
#pragma pack(pop)
```

---

## 附录 B: 系统表关系图

```
┌─────────────────┐
│   sys_tables    │
│   (Page 1)      │
│ ─────────────── │
│ table_id (PK)   │←──┐
│ table_name      │   │
│ root_page       │   │
│ next_rowid      │   │
└─────────────────┘   │
                      │
┌─────────────────┐   │    ┌─────────────────┐
│  sys_columns    │   │    │   sys_indexes   │
│   (Page 2)      │   │    │   (Page 5)      │
│ ─────────────── │   │    │ ─────────────── │
│ table_id (FK) ──┼───┼────│ table_id (FK)   │
│ column_id       │   │    │ index_id (PK)   │
│ column_name     │   │    │ index_name      │
│ data_type       │   │    │ column_id       │
│ nullable        │   │    │ root_page       │
│ is_primary_key  │   │    │ is_unique       │
└─────────────────┘   │    └─────────────────┘
                      │
┌─────────────────┐   │    ┌─────────────────┐
│   sys_users     │   │    │ sys_privileges  │
│   (Page 3)      │   │    │   (Page 4)      │
│ ─────────────── │   │    │ ─────────────── │
│ user_id (PK)  ──┼───┼────│ user_id (FK)    │
│ username        │   └────│ table_id (FK)   │
│ password_hash   │        │ privilege_type  │
│ is_admin        │        └─────────────────┘
└─────────────────┘
                           ┌─────────────────┐
                           │   sys_views     │
                           │   (Page 6)      │
                           │ ─────────────── │
                           │ view_id (PK)    │
                           │ view_name       │
                           │ view_definition │
                           └─────────────────┘
```
