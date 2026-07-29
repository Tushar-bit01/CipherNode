<div align="center">

# TusuDB

### A Lightweight Log-Structured Merge (LSM) Inspired Key-Value Storage Engine

**Modern C++17 • Append-Only Storage • Binary Serialization • SSTables • On-Disk Indexing**

![C++](https://img.shields.io/badge/C++-17-blue)
![Architecture](https://img.shields.io/badge/Architecture-LSM--Inspired-orange)
![Storage](https://img.shields.io/badge/Storage-Append--Only-success)
![Status](https://img.shields.io/badge/Status-Under_Development-yellow)

</div>

---

## Overview

**TusuDB** is a lightweight storage engine written in Modern C++ that explores the core architecture behind Log-Structured Merge (LSM) databases.

Instead of modifying records in-place, every write is appended sequentially to a **Write-Ahead Log (WAL)** before being periodically transformed into immutable **Sorted String Tables (SSTables)**. This design enables efficient sequential writes while maintaining fast lookups through on-disk indexing.

The project is built from scratch to understand how modern storage engines manage persistence, indexing, and immutable data structures at a low level.

---

## Architecture

```text
                    Write Path

              put(key, value)
                     │
                     ▼
              Append to WAL
                     │
                     ▼
        MemTable (In-Memory Index)
                     │
              Flush Threshold
                     │
                     ▼
          Immutable Flushing Map
                     │
                     ▼
             Sort Records by Key
                     │
                     ▼
             Generate SSTable
                     │
                     ▼
          Build On-Disk Index Block
```

```text
                     Read Path

                 get(key)
                    │
                    ▼
             Search MemTable
                    │
          ┌─────────┴─────────┐
          │                   │
       Found             Not Found
          │                   │
          ▼                   ▼
      Read WAL        Search SSTables
                              │
                              ▼
                     Read Footer Metadata
                              │
                              ▼
                     Load Index Block
                              │
                              ▼
                       Binary Search
                              │
                              ▼
                        Read Record
```

---

## Storage Layout

### Write-Ahead Log

```text
+---------+
| Record  |
+---------+
| Record  |
+---------+
| Record  |
+---------+
|   ...   |
+---------+
```

### SSTable

```text
+--------------------------------------+
|                                      |
|        Sorted Data Records           |
|                                      |
+--------------------------------------+
|                                      |
|           Index Block                |
|                                      |
+--------------------------------------+
|                                      |
| Footer (Index Block Offset)          |
|                                      |
+--------------------------------------+
```

---

## Features

- Append-only Write-Ahead Log
- Binary record serialization
- Hash-based MemTable
- Immutable flushing using move semantics
- Sorted String Table generation
- On-disk index blocks
- Binary search based lookups
- Automatic recovery from the WAL
- Modern C++17 implementation
- Zero external database dependencies

---

## Project Structure

```text
TusuDB
│
├── include/
│   ├── engine.h
│   ├── sstable.h
│   ├── binary_storage.h
│   └── binary_record.h
│
├── src/
│   ├── engine.cc
│   ├── sstable.cc
│   └── binary_storage.cc
│   └── main.cc
│
├── docs/
│   ├── design.md    
│
├── tests/
│   ├── bench.cc 
│
├── CMakeLists.txt
│
└── README.md
```

---

## Example

```cpp
#include "engine.h"

int main() {
    TusuEngine db("tusu.db");

    db.put("language", "C++");
    db.put("database", "TusuDB");

    std::cout << db.get("language") << std::endl;
}
```

---

## Build

```bash
git clone https://github.com/<your-username>/TusuDB.git

cd TusuDB

cmake -S . -B build

cmake --build build
```

Run

```bash
./build/engine_test
```

---

## Roadmap

| Feature | Status |
|---------|:------:|
| Write-Ahead Log | ✓ |
| MemTable | ✓ |
| Immutable Flush | ✓ |
| SSTables | ✓ |
| Binary Serialization | ✓ |
| On-Disk Index | ✓ |
| Binary Search | ✓ |
| WAL Recovery | ✓ |
| Tombstones | Planned |
| SSTable Compaction | Planned |
| Bloom Filters | Planned |
| LRU Index Cache | Planned |

---

## Motivation

TusuDB was created as a systems programming project to explore the internal design of modern storage engines through practical implementation. The project focuses on persistence, binary file layouts, immutable storage structures, and efficient on-disk indexing while remaining intentionally lightweight and easy to extend.

---

## References

The architecture is inspired by the design principles of modern Log-Structured Merge Tree databases, including:

- LevelDB
- RocksDB
- Pebble

While inspired by these systems, TusuDB is an independent educational implementation built entirely from scratch.

---

## License

This project is intended for educational and learning purposes.