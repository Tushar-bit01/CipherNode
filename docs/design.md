# TusuDB Design Document

## 1. Project Overview
TusuDB is a high-performance, log-structured storage engine designed for write-heavy workloads. The primary objective is to build a robust, persistent key-value store from the ground up, utilizing modern C++ systems engineering standards.

## 2. Storage Architecture
The system follows a **Log-Structured Merge (LSM)** inspired architecture.

* **Persistence Layer (WAL):**
    * **Structure:** Append-only binary log (`tusu.db`).
    * **Record Format:** `[RecordHeader][Key][Value]`.
    * **Portability:** Structures are enforced with `#pragma pack(push, 1)` to prevent compiler-specific memory padding, ensuring data compatibility across different machine architectures.
* **Memory Management:** * **MemTable:** A `std::unordered_map<std::string, uint64_t>` in RAM acts as the primary index, mapping keys to their byte-offsets on disk.
    * **Performance:** Provides $O(1)$ average time complexity for both `put` and `get` operations.

## 3. Implementation Details
* **Offset Management:** Every write operation returns a `uint64_t` byte-offset (via `tellp()`), allowing the `Engine` to perform random-access reads (`seekg()`) on the disk file.
* **Engine Controller:**
    * Encapsulates the interaction between the physical storage layer and the in-memory index.
    * Ensures atomic-like consistency by appending to the WAL before updating the MemTable.

## 4. Current Development Status
* **[x] Stage 1: Core I/O:** WAL implementation completed.
* **[x] Stage 2: Offset Management:** Writer refactored to support direct addressing.
* **[ ] Stage 3: Engine Integration:** `TusuEngine` implementation (RAM indexing + Random Access Reads).
* **[ ] Stage 4: Compaction & Recovery:** Future phase involving SSTable generation and startup log-replay.

## 5. Design Trade-offs
* **`uint64_t` vs `uint32_t` for Offsets:** Used `uint64_t` to ensure the engine can handle data files exceeding 4GB, future-proofing for large-scale production datasets.
* **Append-Only vs. In-Place Update:** Chose append-only writes to maximize I/O throughput, accepting that periodic compaction is required to manage "dead" (stale) data.