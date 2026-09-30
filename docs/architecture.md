# TusuDB Architecture

TusuDB is a lightweight **LSM-inspired key-value storage engine** written in Modern C++. It uses an append-only WAL, an in-memory MemTable, immutable SSTables, asynchronous flushing, and background compaction.

## 1. Architecture Overview

```text
                         Client
                           │
              ┌────────────┴────────────┐
              │                         │
             PUT                       GET
              │                         │
              ▼                         ▼
        Append to WAL            Active MemTable
              │                         │
              ▼                    Not Found
        Update MemTable                  │
              │                          ▼
      Flush threshold              Flush Queue
              │                          │
              ▼                    Flushing Batch
       Flush Queue                      │
              │                    Not Found
              ▼                          │
      Background Flush                  ▼
              │                    SSTable Search
              ▼                          │
          SSTable                  Newest → Oldest
              │                          │
              ▼                          ▼
       Background                 Index → Record
       Compaction                       │
                                        ▼
                                      Value
```

### Core Components

| Component | Responsibility |
|---|---|
| WAL | Durable append-only persistence |
| MemTable | Active in-memory key → WAL offset index |
| Flush Queue | Holds immutable batches waiting for persistence |
| Flushing Batch | Batch currently being written to an SSTable |
| SSTable | Immutable sorted on-disk storage |
| Index Cache | Cached SSTable index blocks |
| Flush Worker | Asynchronous SSTable generation |
| Compaction | Background SSTable merging |

---

## 2. Write-Ahead Log

The WAL is the first persistent destination for writes.

Current WAL:

```text
tusu.db
```

Records use a binary layout:

```text
+----------------+
| Record Header  |
+----------------+
| Key Bytes      |
+----------------+
| Value Bytes    |
+----------------+
```

The header contains:

```text
keySize
valueSize
```

Writes are append-only. Existing records are never modified in place.

The WAL also acts as the recovery source during startup.

---

## 3. MemTable

The active MemTable stores:

```text
key → WAL byte offset
```

Example:

```text
"Apple" → 0
"Cat"   → 41
"Dog"   → 97
```

The offset identifies the latest WAL record for the key.

The current MemTable uses a hash table, providing average O(1) insertion and lookup.

Sorting is deferred until the flush stage.

---

## 4. Asynchronous Flush Pipeline

When the active MemTable reaches the flush threshold:

```text
Active MemTable
      │
      │ std::move()
      ▼
Flush Queue
      │
      ▼
Background Flush Worker
      │
      ▼
SSTable
```

The active MemTable is replaced immediately so new writes can continue while the previous batch is being persisted.

A batch moves through:

```text
Active MemTable
      ↓
Flush Queue
      ↓
Flushing Batch
      ↓
SSTable
```

---

## 5. Write Path

```text
PUT
 │
 ▼
Append to WAL
 │
 ▼
Get WAL Offset
 │
 ▼
Update MemTable
 │
 ▼
Flush Threshold?
 ├── No  → Return
 │
 └── Yes
       │
       ▼
   Move MemTable
       │
       ▼
   Flush Queue
       │
       ▼
   Notify Worker
       │
       ▼
     Return
```

SSTable generation happens asynchronously rather than inside the normal foreground PUT path.

---

## 6. SSTable Generation

An immutable batch is converted into a sorted SSTable:

```text
Immutable Batch
      │
      ▼
Extract Keys
      │
      ▼
Sort Keys
      │
      ▼
Read Records From WAL
      │
      ▼
Write Sorted Records
      │
      ▼
Build Index Block
      │
      ▼
Write Footer
      │
      ▼
Publish SSTable
```

Sorting is therefore removed from the normal write path.

---

## 7. SSTable Layout

```text
+--------------------------------+
| Sorted Data Records            |
+--------------------------------+
| Index Block                    |
+--------------------------------+
| Footer                         |
| Index Start Offset             |
+--------------------------------+
```

### Data Region

Contains serialized records in lexicographical key order.

### Index Block

Stores key-to-record-offset mappings:

```text
Apple  → 0
Ball   → 37
Cat    → 79
Dog    → 121
```

The sorted index supports binary search.

### Footer

Stores the starting offset of the index block, allowing direct index discovery.

---

## 8. Read Path

GET searches newer data before older data:

```text
GET(key)
   │
   ▼
Active MemTable
   │
   ├── Found ───────────────► Return
   │
   ▼
Flush Queue
   │
   ▼
Flushing Batch
   │
   ├── Found ───────────────► Return
   │
   ▼
SSTables
   │
   ▼
Newest → Oldest
   │
   ▼
Index Cache / Index
   │
   ▼
Binary Search
   │
   ▼
Record Offset
   │
   ▼
Read Record
   │
   ▼
Return Value
```

Searching newest-to-oldest preserves the latest version of a key.

---

## 9. Index Cache

TusuDB keeps SSTable indexes in memory:

```text
SSTable
   │
   ▼
Index Block
   │
   ▼
index_cache
```

This avoids repeatedly loading the same index block from disk.

The index cache is protected by the SSTable metadata synchronization mechanism.

---

## 10. Compaction

Background compaction merges SSTables and removes obsolete versions.

```text
SSTable A ─┐
SSTable B ─┼──► Compaction ──► New SSTable
SSTable C ─┘
```

The resulting SSTable is published as the new persistent state, while obsolete SSTables are removed safely.

Compaction is separated from the foreground request path.

---

## 11. Data Visibility

During asynchronous flushing, a record may exist in:

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

> A successfully written record must remain discoverable throughout its transition from memory to persistent storage.

This prevents GET from incorrectly returning `not found` while a record is being flushed.

---

## 12. Concurrency Model

The current engine uses concurrency primarily for background storage work.

```text
                    TusuDB
                       │
          ┌────────────┼────────────┐
          │            │            │
          ▼            ▼            ▼
     Foreground    Flush Worker  Compaction
     Operations       Thread       Worker
```

Current synchronization:

| Primitive | Responsibility |
|---|---|
| `std::mutex` | Flush queue, flushing batch, shutdown state |
| `std::condition_variable` | Flush worker coordination |
| `std::shared_mutex` | SSTable metadata and index cache |

The public PUT/GET/REMOVE API is not yet designed as a fully concurrent multi-client API. That is a future optimization stage.

---

## 13. Recovery

On startup, TusuDB reconstructs the active MemTable from the WAL.

```text
Open WAL
   │
   ▼
Read Record
   │
   ▼
Extract Key
   │
   ▼
Store Latest Offset
   │
   ▼
Repeat Until EOF
   │
   ▼
Reconstructed MemTable
```

If a key appears multiple times, the latest WAL record becomes the active offset.

Existing SSTables are also discovered during startup and their index metadata is loaded.

---

## 14. Storage Lifecycle

A record progresses through:

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

The architecture separates mutable foreground state from immutable persistent state.

---

## 15. Current Design State

| Area | Status |
|---|---|
| Binary WAL | Implemented |
| Hash-based MemTable | Implemented |
| Immutable flush batches | Implemented |
| Background flushing | Implemented |
| SSTable index | Implemented |
| Index cache | Implemented |
| Background compaction | Implemented |
| WAL recovery | Implemented |
| SSTable metadata synchronization | Implemented |
| Fully concurrent client API | Future work |
| Bloom filters | Future work |
| Data/block cache | Future work |
| Advanced concurrency optimization | Future work |

---

## 16. Optimization Direction

The core storage architecture is now established. Further work should focus on **measurement and optimization rather than continuously adding features**.

The main areas are:

1. Profile the current read/write paths.
2. Investigate the current GET bottleneck.
3. Add Bloom filters and measure their effect.
4. Add data/block caching if profiling justifies it.
5. Introduce concurrent client access.
6. Measure scaling across multiple threads.
7. Identify and reduce contention.
8. Stress-test concurrency, flushing, compaction, and recovery.

The goal is to understand and optimize the existing architecture rather than continuously expanding its feature set.
