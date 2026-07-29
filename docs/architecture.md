# TusuDB Storage Engine Architecture Specification

TusuDB is a lightweight **Log-Structured Merge (LSM) inspired** key-value storage engine implemented in Modern C++. The project explores the core architectural principles behind modern storage systems by combining append-only persistence, immutable in-memory structures, binary serialization, and sorted on-disk tables.

Rather than focusing on production-scale optimizations, TusuDB emphasizes understanding the complete storage lifecycle—from accepting writes in memory to organizing immutable disk structures for efficient retrieval.

---

# 1. Architectural Overview

TusuDB follows a write-optimized storage architecture built around three primary components.

- **Write-Ahead Log (WAL)** for durable persistence
- **MemTable** for fast in-memory indexing
- **Sorted String Tables (SSTables)** for immutable on-disk storage

```text
                      Client

                         │
          ┌──────────────┴──────────────┐
          │                             │
          ▼                             ▼
       put(key)                     get(key)
          │                             │
          ▼                             ▼
   Append Record                  Search MemTable
      to WAL                           │
          │                       Not Found
          ▼                             │
     Update MemTable                    ▼
          │                     Search SSTables
          ▼                             │
     Flush Threshold                    ▼
          │                      Return Value
          ▼
 Generate SSTable
```

The storage engine is append-only. Existing records are never modified in-place. Every update creates a new version of the record which is appended to persistent storage.

---

# 2. Storage Components

## Write-Ahead Log (WAL)

The Write-Ahead Log is the primary persistence layer of the engine.

Every write operation is immediately appended to a binary file before any additional processing occurs. This guarantees that records are durably stored before eventually being flushed into immutable SSTables.

Current implementation

```text
tusu.db
```

The WAL stores records sequentially.

```text
+---------+
| Record  |
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

Sequential writes minimize disk seek operations while simplifying recovery.

---

## MemTable

The MemTable acts as the active in-memory index.

Instead of storing complete records, it maintains references to the latest version of every key inside the WAL.

Conceptually

```text
Key
 │
 ▼
Record Offset
```

Example

```text
Apple

↓

Offset 0


Cat

↓

Offset 41


Dog

↓

Offset 97
```

Using offsets instead of complete records significantly reduces memory consumption while keeping retrieval efficient.

---

## Immutable Flushing Map

During flushing, ownership of the active MemTable is transferred into a dedicated immutable structure.

```text
Active MemTable

        │

        ▼

std::move()

        │

        ▼

Immutable Flushing Map
```

The flushing map represents a consistent snapshot of the current database state and remains unchanged until SSTable generation completes.

This separation allows the active MemTable to begin accepting new writes immediately after ownership transfer.

---

# 3. Storage Lifecycle

Every key-value pair progresses through three distinct stages during its lifetime.

```text
             In-Memory

         Active MemTable

                │

                ▼

        Immutable Snapshot

                │

                ▼

           Sorted SSTable

                │

                ▼

       Persistent Disk Storage
```

Each transition improves durability while maintaining efficient write throughput.

---

# 4. Binary Record Model

Every record written by TusuDB follows a deterministic binary layout.

```text
+--------------------------------+
| Record Header                  |
+--------------------------------+
| Key Bytes                      |
+--------------------------------+
| Value Bytes                    |
+--------------------------------+
```

The header stores metadata describing the variable-length key and value.

```text
RecordHeader

───────────────

keySize

valueSize
```

The engine serializes records directly into contiguous binary memory without introducing textual formatting or parsing overhead.

This layout allows records to be reconstructed efficiently during both recovery and lookups.

---

# 5. Memory Model

TusuDB deliberately separates transient memory structures from persistent storage.

```text
                 RAM

        Active MemTable

        Flushing Map



-------------------------------



                Disk

      Write Ahead Log

      SSTable 1

      SSTable 2

      SSTable N
```

The MemTable and flushing map exist only during runtime.

The WAL and SSTables represent the durable state of the database.

---

# 6. Binary Serialization Philosophy

The storage engine exclusively uses binary serialization for persistence.

Instead of storing human-readable text, every structure is written exactly as contiguous bytes.

```text
Memory

↓

Binary Representation

↓

Disk
```

This approach provides several advantages.

- Compact storage
- Predictable layouts
- Sequential writes
- Minimal parsing overhead
- Direct reconstruction into memory

Binary serialization forms the foundation for every storage structure implemented within TusuDB.

---

# 7. Data Ownership

Ownership of data changes throughout the write pipeline.

```text
Client

        │

        ▼

MemTable

        │

        ▼

Flushing Map

        │

        ▼

SSTable

        │

        ▼

Persistent Storage
```

Only one component owns a mutable copy of the data at any point in time.

Once an SSTable has been generated, it becomes immutable and is never modified again.

---

# 8. Design Principles

The architecture is guided by several core principles.

### Append-Only Persistence

Existing records are never overwritten.

Every update creates a new serialized record appended to disk.

---

### Immutable Storage

Once flushed, an SSTable is considered read-only.

Future updates generate entirely new storage structures rather than modifying existing files.

---

### Sequential Disk Access

The engine favors sequential writes over random writes.

This design aligns with modern storage engines where sequential I/O generally provides significantly better throughput than arbitrary updates.

---

### Separation of Responsibilities

Each subsystem performs a single responsibility.

| Component | Responsibility |
|-----------|----------------|
| WAL | Durable append-only persistence |
| MemTable | In-memory indexing |
| Flushing Map | Immutable snapshot |
| SSTable | Persistent sorted storage |
| Index Block | Efficient record discovery |

---

# 9. High-Level Write Pipeline

```text
Client Write

        │

        ▼

Serialize Record

        │

        ▼

Append to WAL

        │

        ▼

Update MemTable

        │

Flush Threshold

        ▼

Move Ownership

        │

        ▼

Sort Keys

        │

        ▼

Generate SSTable

        │

        ▼

Persist Index Block

        │

        ▼

Flush Complete
```

The write path prioritizes sequential disk writes while delaying sorting until flush time.

---

# 10. SSTable Architecture

The Sorted String Table (SSTable) is the primary persistent storage structure within TusuDB.

Unlike the Write-Ahead Log, which preserves insertion order, an SSTable stores records in **lexicographical key order**, allowing efficient logarithmic-time lookups.

Once created, an SSTable is immutable and is never modified in-place.

```text
                Flush

                  │

                  ▼

          Immutable Snapshot

                  │

                  ▼

          Sort All Keys

                  │

                  ▼

        Generate SSTable

                  │

                  ▼

      Persist Index Metadata
```

The immutable nature of SSTables eliminates synchronization concerns during reads while simplifying the overall storage model.

---

# 11. SSTable Generation Pipeline

Since the active MemTable is implemented as a hash table, its contents are inherently unordered.

Before persistence, the engine transforms this unordered representation into a sorted on-disk structure.

```text
         Flushing Map

      (unordered_map)

             │

             ▼

      Extract All Keys

             │

             ▼

     Lexicographical Sort

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
```

Sorting occurs only once during flushing, allowing write operations to remain constant-time while enabling efficient read performance.

---

# 12. SSTable Physical Layout

Each SSTable consists of three independent regions.

```text
+------------------------------------------------------+
|                                                      |
|               Sorted Data Records                    |
|                                                      |
+------------------------------------------------------+
|                                                      |
|                 Index Block                          |
|                                                      |
+------------------------------------------------------+
|                                                      |
|          Footer (Index Block Offset)                 |
|                                                      |
+------------------------------------------------------+
```

Each region serves a dedicated purpose.

| Region | Purpose |
|---------|---------|
| Data Region | Stores serialized key-value records |
| Index Block | Maps keys to record offsets |
| Footer | Stores the starting position of the index block |

---

# 13. Data Region

The data region contains every serialized record ordered lexicographically by key.

Example

```text
Apple

Ball

Cat

Dog

Orange

Zebra
```

Each record follows the same binary layout.

```text
Record Header

↓

Key Bytes

↓

Value Bytes
```

The engine writes records sequentially, ensuring the file remains contiguous.

---

# 14. Index Block

The index block provides a mapping between keys and their corresponding locations inside the SSTable.

Conceptually

```text
Key

↓

Record Offset
```

Example

```text
Apple     →      0

Ball      →     37

Cat       →     79

Dog       →    121

Orange    →    168
```

The offsets always reference the beginning of a serialized record inside the same SSTable.

Because entries are stored in sorted order, binary search can be performed directly over the index.

---

# 15. Footer

The footer occupies the final bytes of every SSTable.

Its purpose is to locate the index block without scanning the file sequentially.

```text
+----------------------------+
| Index Start Offset         |
+----------------------------+
```

Lookup begins by reading the footer.

Once the index offset has been recovered, the engine immediately jumps to the beginning of the index block.

```text
Open SSTable

      │

      ▼

Read Footer

      │

      ▼

Locate Index Block

      │

      ▼

Binary Search
```

This design allows the index to remain physically separated from the data region while still supporting direct access.

---

# 16. Read Pipeline

The read path follows a layered lookup strategy.

Recently written data is searched before older persistent structures.

```text
               Client

                  │

                  ▼

          Search MemTable

                  │

      ┌───────────┴───────────┐
      │                       │
      ▼                       ▼

   Found                 Not Found

                              │

                              ▼

                  Search SSTables

                              │

                              ▼

                Newest → Oldest

                              │

                              ▼

                   Read Footer

                              │

                              ▼

                Locate Index Block

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

Searching newer SSTables first guarantees that the latest version of a key is returned.

---

# 17. Lookup Strategy

The lookup algorithm minimizes unnecessary disk reads.

Rather than scanning every serialized record, the engine performs the following sequence.

```text
Open SSTable

        │

        ▼

Read Footer

        │

        ▼

Jump to Index

        │

        ▼

Load Index

        │

        ▼

Binary Search

        │

        ▼

Record Offset

        │

        ▼

Jump to Record

        │

        ▼

Deserialize Value
```

Only the matching record is read from the data region.

The remaining records remain untouched.

---

# 18. Index Search

The index block behaves as a sorted array of key-offset pairs.

```text
Apple

Ball

Cat

Dog

Fox

Orange

Zebra
```

Binary search repeatedly halves the remaining search space until the requested key is located.

```text
Entire Index

      │

      ▼

Middle Entry

      │

      ▼

Discard Half

      │

      ▼

Repeat

      │

      ▼

Matching Offset
```

The resulting offset is then used to retrieve the serialized record directly.

---

# 19. Storage Evolution

Every record transitions through several storage representations during its lifetime.

```text
Client

      │

      ▼

Serialized WAL Record

      │

      ▼

MemTable Entry

      │

      ▼

Immutable Snapshot

      │

      ▼

Sorted SSTable

      │

      ▼

Indexed Persistent Storage
```

Each stage progressively improves durability while maintaining efficient write characteristics.

---

# 20. Architectural Trade-offs

The current implementation intentionally favors simplicity over advanced optimization.

## Current Design

- Entire index block is loaded during lookup.
- Keys are fully sorted during flush.
- Records remain immutable after persistence.
- SSTables are searched sequentially from newest to oldest.

These decisions reduce implementation complexity while preserving the core architecture of an LSM-inspired storage engine.

Future iterations will extend this foundation with more sophisticated indexing and storage techniques.

---
# 21. Recovery Model

TusuDB reconstructs its in-memory state by replaying the Write-Ahead Log during startup.

Instead of storing a serialized MemTable, the engine scans every record inside the WAL and rebuilds the latest key-to-offset mapping.

```text
           Engine Startup

                  │

                  ▼

           Open WAL File

                  │

                  ▼

        Read Record Header

                  │

                  ▼

            Read Key Bytes

                  │

                  ▼

          Skip Value Bytes

                  │

                  ▼

      Update Latest Key Offset

                  │

                  ▼

          Repeat Until EOF

                  │

                  ▼

      Reconstructed MemTable
```

Because newer records overwrite older offsets, the reconstructed MemTable always points to the latest version of every key.

---

# 22. Write Path Design

The write path prioritizes sequential disk writes while avoiding expensive sorting during normal operation.

```text
Client Write

      │

      ▼

Serialize Record

      │

      ▼

Append to WAL

      │

      ▼

Update MemTable

      │

      ▼

Flush Threshold Reached?

      │

      ├────────── No ──────────► Return

      ▼

Create Immutable Snapshot

      │

      ▼

Generate SSTable
```

Sorting is intentionally deferred until flush time.

This keeps the common write path lightweight while still allowing SSTables to remain fully sorted.

---

# 23. Read Path Design

Lookups always prioritize the newest data.

```text
                Lookup

                   │

                   ▼

             Active MemTable

                   │

          Found? ─────────────► Return

                   │

                   ▼

         Search SSTables

                   │

                   ▼

      Newest File First

                   │

                   ▼

         Binary Search Index

                   │

                   ▼

          Read Matching Record
```

Searching newer SSTables first guarantees visibility of the latest persisted value.

---

# 24. Design Decisions

## Append-Only Persistence

The engine never modifies records in-place.

Instead, updates are appended as new records.

Advantages

- Sequential writes
- Simpler persistence
- Easier recovery
- Predictable storage layout

---

## Hash-Based MemTable

The MemTable is intentionally implemented as a hash table.

Advantages

- Average constant-time inserts
- Average constant-time lookups
- Minimal write latency

The unordered nature of the MemTable is compensated by sorting during flush.

---

## Immutable SSTables

Every generated SSTable becomes read-only.

Advantages

- Simplifies storage management
- Eliminates accidental modification
- Provides deterministic file layouts

---

## Binary Serialization

Records are persisted exactly as binary bytes.

Advantages

- Compact representation
- No parsing overhead
- Deterministic layouts
- Efficient sequential I/O

---

## Footer-Based Index Discovery

The engine stores the starting position of the index block inside the footer.

Advantages

- Constant-time index discovery
- No sequential scanning
- Flexible file layout

---

# 25. Current Limitations

The current implementation intentionally focuses on the core storage engine architecture.

Several production-oriented optimizations are not yet implemented.

| Component | Current Status |
|-----------|----------------|
| Tombstones | Not Implemented |
| Compaction | Not Implemented |
| Bloom Filters | Not Implemented |
| LRU Cache | Not Implemented |
| Background Flush | Not Implemented |
| Concurrent Writes | Not Implemented |

These features are planned as future extensions rather than part of the initial architecture.

---

# 26. Future Architecture

## Tombstones

Delete operations will be represented using tombstone records rather than immediately removing data.

```text
Delete("Apple")

        │

        ▼

Append Tombstone

        │

        ▼

Future Compaction

        │

        ▼

Remove Obsolete Records
```

This preserves append-only semantics while supporting logical deletion.

---

## SSTable Compaction

As more SSTables accumulate, lookup cost gradually increases.

Compaction merges multiple SSTables into a single larger table while discarding obsolete record versions.

```text
 SSTable A

 SSTable B

 SSTable C

        │

        ▼

 Merge

        │

        ▼

 Remove Older Versions

        │

        ▼

 New SSTable
```

Compaction reduces storage redundancy and improves read performance.

---

## Bloom Filters

Without Bloom filters, every SSTable may need to be searched during a lookup.

Bloom filters provide a probabilistic membership test.

```text
Lookup Key

      │

      ▼

Bloom Filter

      │

      ├──────── Definitely Not Present

      │

      └──────── Possibly Present

                    │

                    ▼

             Search SSTable
```

This significantly reduces unnecessary disk accesses.

---

## LRU Index Cache

The current implementation loads an SSTable index whenever a lookup is performed.

Future versions will cache recently accessed indexes.

```text
Lookup

    │

    ▼

LRU Cache

    │

    ├──────── Cache Hit

    │

    ▼

Use Cached Index

    │

    └──────── Cache Miss

              │

              ▼

Load Index From Disk

              │

              ▼

Insert Into Cache
```

Caching frequently accessed indexes reduces repeated disk I/O while limiting memory consumption.

---

# 27. Design Philosophy

TusuDB intentionally prioritizes architectural clarity over feature completeness.

Each subsystem is designed to represent a single storage concept with minimal abstraction, making the complete write and read lifecycle easy to understand.

The implementation emphasizes

- deterministic binary storage
- immutable data structures
- sequential disk writes
- explicit ownership transfer
- predictable file layouts

rather than maximizing throughput or minimizing latency.

This philosophy makes TusuDB an educational implementation of storage engine fundamentals while providing a solid foundation for progressively introducing more advanced LSM-tree techniques.

---

# 28. Conclusion

TusuDB demonstrates how a modern storage engine can be constructed from a small collection of well-defined components.

Beginning with an append-only Write-Ahead Log, the engine maintains an in-memory index for fast writes, periodically materializes immutable Sorted String Tables, and performs efficient lookups through persistent on-disk indexes.

Although intentionally lightweight, the architecture captures many of the core principles employed by contemporary log-structured databases and establishes a foundation for future additions including tombstones, compaction, Bloom filters, and caching.

The project serves as both a practical systems programming exercise and an exploration of the design decisions underlying modern storage engines.

---