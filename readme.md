<div align="center">

# TusuDB

### A Lightweight LSM-Inspired Key-Value Storage Engine

**Modern C++17 • Append-Only WAL • Immutable SSTables • On-Disk Indexing • Background Compaction**

![C++](https://img.shields.io/badge/C++-17-blue)
![Architecture](https://img.shields.io/badge/Architecture-LSM--Inspired-orange)
![Storage](https://img.shields.io/badge/Storage-Append--Only-success)
![Status](https://img.shields.io/badge/Status-Under_Development-yellow)

</div>

---

## Overview

**TusuDB** is a key-value storage engine implemented from scratch in Modern C++17.

It uses an **LSM-inspired storage architecture** where writes are appended to a binary Write-Ahead Log (WAL), indexed in memory, and asynchronously materialized into immutable Sorted String Tables (SSTables).

The engine is built around a small set of storage-engine primitives:

- Append-only persistence
- In-memory indexing
- Immutable on-disk files
- Sorted SSTables
- Persistent index blocks
- Background flushing
- SSTable compaction
- WAL recovery
- Explicit synchronization between foreground operations and background storage work

The project is intended to explore the implementation and trade-offs involved in building a storage engine rather than hiding those details behind an existing database library.

---

## Architecture

### High-Level Architecture

```text
                             TusuDB
                                │
                ┌───────────────┴───────────────┐
                │                               │
               PUT                             GET
                │                               │
                ▼                               ▼
           Append WAL                    Active MemTable
                │                               │
                ▼                          Not Found
          Update MemTable                       │
                │                               ▼
         Flush Threshold                  Flush Queue
                │                               │
                ▼                         Flushing Batch
          Flush Queue                            │
                │                           Not Found
                ▼                               │
       Background Flush                         ▼
                │                        SSTable Search
                ▼                        Newest → Oldest
             SSTable                            │
                │                               ▼
                ▼                       Index → Binary Search
          Compaction                            │
                                                ▼
                                              Value
```

---

## Write Path

A write is first persisted to the WAL and then represented in the active MemTable by its WAL byte offset.

```text
put(key, value)
      │
      ▼
Append Binary Record to WAL
      │
      ▼
Get WAL Byte Offset
      │
      ▼
Update Active MemTable
      │
      ├──────── below threshold ────────► Return
      │
      ▼
Move MemTable into Flush Queue
      │
      ▼
Notify Flush Worker
      │
      ▼
Return
```

The active MemTable can continue accepting writes while an older immutable batch is being flushed.

### WAL Record

```text
+----------------+
| Record Header  |
+----------------+
| Key Bytes      |
+----------------+
| Value Bytes    |
+----------------+
```

The binary record header stores the key and value sizes and the metadata required by the storage layer.

---

## Asynchronous Flush

When the active MemTable reaches the configured threshold, ownership of the map is transferred into the flush queue using move semantics.

```text
Active MemTable
      │
      │ move
      ▼
Flush Queue
      │
      ▼
Flush Worker
      │
      ├── Sort Keys
      ├── Read Records from WAL
      ├── Write Sorted Records
      ├── Build Index Block
      └── Write Footer
             │
             ▼
          SSTable
```

This separates the foreground write path from SSTable generation.

The engine keeps track of the active MemTable, queued batches, and the batch currently being flushed so that records remain visible throughout the asynchronous transition.

---

## Read Path

```text
get(key)
   │
   ▼
Active MemTable
   │
   ├── Found ──────────────► Read WAL
   │
   ▼
Flush Queue
   │
   ▼
Flushing Batch
   │
   ▼
SSTables
(Newest → Oldest)
   │
   ▼
Index Cache / Index Block
   │
   ▼
Binary Search
   │
   ▼
Read Record
   │
   ▼
Return Value
```

SSTables are searched from newest to oldest so that the newest version of a key takes precedence over older versions.

---

## SSTable Format

Each SSTable contains sorted records followed by an index block and a footer.

```text
+--------------------------------------+
|                                      |
|          Sorted Data Records         |
|                                      |
+--------------------------------------+
|                                      |
|             Index Block              |
|                                      |
+--------------------------------------+
|                                      |
|       Footer: Index Offset           |
|                                      |
+--------------------------------------+
```

The footer provides the location of the index block.

The index stores keys and their corresponding file offsets, allowing the engine to locate records through binary search instead of scanning the entire SSTable.

---

## Compaction

SSTables are immutable after publication.

As multiple SSTables accumulate, compaction merges them into a new SSTable while resolving obsolete versions.

```text
SSTable A ─┐
SSTable B ─┼──► Compaction ──► New SSTable
SSTable C ─┘
```

The new SSTable is published and the obsolete SSTables are removed from the active metadata set.

Compaction is performed as part of the background storage pipeline rather than directly inside the client-facing PUT/GET path.

---

## Data Visibility

Asynchronous flushing creates multiple temporary locations from which a key may be readable:

```text
Active MemTable
      OR
Flush Queue
      OR
Flushing Batch
      OR
Published SSTable
```

The important invariant is:

> A successfully written record remains discoverable while it moves from the active MemTable to persistent SSTable storage.

This prevents asynchronous flushing from creating a visibility gap for GET operations.

---

## Index Cache

TusuDB keeps SSTable index blocks in memory:

```text
SSTable
   │
   ▼
Index Block
   │
   ▼
Index Cache
```

The cached index allows repeated lookups to avoid re-reading the index block from disk.

SSTable metadata and the index cache are synchronized separately from the flush queue.

---

## Recovery

The WAL provides the recovery source for the in-memory state.

```text
Open WAL
   │
   ▼
Read Record
   │
   ▼
Extract Key + Offset
   │
   ▼
Update MemTable
   │
   ▼
Repeat Until EOF
   │
   ▼
Recovered MemTable
```

If a key occurs multiple times in the WAL, the latest record becomes the active MemTable entry.

Existing SSTables are discovered during startup and their index metadata is loaded into the engine.

---

## Concurrency Model

Concurrency is currently used for background storage work rather than exposing a fully concurrent public API.

```text
                     TusuDB
                        │
          ┌─────────────┼─────────────┐
          │             │             │
          ▼             ▼             ▼
     Foreground     Flush Worker   Compaction
     Operations        Thread        Work
```

Synchronization currently includes:

| Primitive | Responsibility |
|---|---|
| `std::mutex` | Flush queue, flushing batch, shutdown state |
| `std::condition_variable` | Flush worker coordination |
| `std::shared_mutex` | SSTable metadata and index cache |

The public API is currently structured around a foreground client path with background storage work. Multi-client concurrent access is a separate stage of the implementation.

---

## Storage Lifecycle

```text
Client
  │
  ▼
WAL
  │
  ▼
Active MemTable
  │
  ▼
Flush Queue
  │
  ▼
Flushing Batch
  │
  ▼
Sorted SSTable
  │
  ▼
Compaction
  │
  ▼
Immutable Persistent Storage
```

The architecture separates mutable in-memory state from immutable persistent state and moves data between them through explicit ownership and synchronization boundaries.

---

## Features

- Append-only binary Write-Ahead Log
- Binary record serialization
- Hash-based MemTable
- WAL byte-offset indexing
- Immutable flush batches
- Move-based ownership transfer during flushing
- Asynchronous background flushing
- Sorted String Table generation
- On-disk index blocks
- In-memory SSTable index cache
- Binary-search based lookups
- Tombstone-based deletion
- SSTable compaction
- WAL-based recovery
- SSTable metadata synchronization
- Modern C++17 implementation
- Zero external database dependencies

---

## Project Structure

```text
TusuDB/
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
│   ├── binary_storage.cc
│   └── main.cc
│
├── docs/
│   └── design.md
│
├── tests/
│   └── bench.cc
│
├── CMakeLists.txt
└── README.md
```

---

## Example

```cpp
#include "engine.h"
#include <iostream>

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

Run:

```bash
./build/engine_test
```

---

## Benchmarking

TusuDB includes a benchmark harness for evaluating the current storage path.

Current workloads include:

- Sequential PUT
- Random PUT
- Existing-key GET
- Missing-key GET
- 50/50 Mixed workload

The benchmark records:

- Total execution time
- Throughput
- p50 latency
- p95 latency
- p99 latency
- WAL size
- SSTable size
- Total database size

Benchmark results are kept separately from the source tree:

```text
benchmark_results/
├── baseline_v0.csv
├── baseline_v1.csv
└── baseline_v2.csv
```

The benchmark suite is used to compare changes to the storage engine under controlled workloads.

---

## Development Direction

The current architecture provides the foundation for the next stage of the project: measuring the existing implementation and improving its storage and read paths based on observed behavior.

Areas being evaluated include:

- Read-path bottlenecks
- Bloom filters for SSTable rejection
- Data/block caching
- Concurrent client access
- Synchronization overhead
- Flush and compaction behavior
- Scaling across multiple client threads
- Recovery behavior under larger WALs

Each change can be evaluated against the benchmark suite to determine its effect on throughput, latency, storage footprint, and scalability.

---

## Motivation

TusuDB is a systems programming project focused on implementing storage-engine fundamentals directly.

The project explores:

- Binary file formats
- Disk I/O
- Write-Ahead Logging
- In-memory indexing
- Immutable storage
- SSTable design
- On-disk indexing
- Compaction
- Caching
- Concurrency
- Crash recovery

The implementation is intentionally compact enough that the complete path from a client operation to the underlying bytes on disk can be understood and inspected.

---

## References

TusuDB draws architectural inspiration from LSM-based storage engines such as:

- LevelDB
- RocksDB
- Pebble

TusuDB is an independent implementation written from scratch for systems programming and storage-engine experimentation.

---

## License

This project is intended for educational and learning purposes.
