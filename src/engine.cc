#include "../include/engine.h"
#include "../include/sstable.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>

TusuEngine::TusuEngine(const std::string &filename) : db_file(filename)
{
    std::ifstream infile(db_file, std::ios::binary);

    // Recover MemTable from WAL if it exists
    if (infile.is_open())
    {
        RecordHeader header;

        while (true)
        {
            uint64_t current_offset = infile.tellg();

            if (!infile.read(
                    reinterpret_cast<char *>(&header),
                    sizeof(RecordHeader)))
                break;

            std::string key(header.keySize, '\0');

            infile.read(&key[0], header.keySize);

            infile.seekg(header.valueSize, std::ios::cur);

            memtable[key] = current_offset;
        }
    }

    // Discover existing SSTables
    for (const auto &entry : std::filesystem::directory_iterator("."))
    {
        std::string filename = entry.path().filename().string();

        if (filename.rfind("sstable_", 0) == 0 &&
            entry.path().extension() == ".db")
        {
            sstable_files.push_back(filename);
            std::ifstream file(filename, std::ios::binary);

            if (file.is_open())
            {
                index_cache[filename] = readIndexBlock(file);
            }
        }
    }
    std::sort(sstable_files.begin(), sstable_files.end());

    flush_thread = std::thread(&TusuEngine::flushWorker, this);
}

TusuEngine::~TusuEngine()
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        shutting_down = true;
    }

    flush_cv.notify_one();
    flush_thread.join();
}

void TusuEngine::flush(std::unordered_map<std::string, uint64_t> &batch)
{
    std::vector<std::string> sorted_keys;
    sorted_keys.reserve(batch.size());
    for (const auto &pair : batch)
    {
        sorted_keys.push_back(pair.first);
    }
    std::sort(sorted_keys.begin(), sorted_keys.end());
    SSTableResult result = writeSStable(sorted_keys, batch);

    {
        std::unique_lock lock(sstable_mtx);

        sstable_files.push_back(result.filename);
        index_cache[result.filename] = std::move(result.index);
    }
    CompactionResult compacted = checkAndCompactSSTables(sstable_files, index_cache);
    if (compacted.compacted)
    {
        std::unique_lock lock(sstable_mtx);
        // Remove old SSTables and their cache entries.
        for (const auto &filename : compacted.old_files)
        {
            std::remove(filename.c_str());
            index_cache.erase(filename);
        }

        // Remove the first 8 old SSTables.
        sstable_files.erase(
            sstable_files.begin(),
            sstable_files.begin() + compacted.old_files.size());

        index_cache[compacted.filename] = std::move(compacted.index);
        // Compacted SSTable represents the old range,
        // so keep it before newer SSTables.
        sstable_files.insert(
            sstable_files.begin(),
            compacted.filename);
    }
}

void TusuEngine::flushWorker()
{
    while (true)
    {
        std::unordered_map<std::string, uint64_t> batch;

        {
            std::unique_lock<std::mutex> lock(mtx);

            flush_cv.wait(lock, [this]
                          { return shutting_down || !flush_queue.empty(); });

            if (shutting_down && flush_queue.empty())
                return;

            batch = std::move(flush_queue.front());
            flush_queue.pop();
        }
        flush(batch);
    }
}

void TusuEngine::put(const std::string &key, const std::string &value)
{
    uint64_t offset = writeRecord(db_file, key, value);
    memtable[key] = offset;
    if (memtable.size() >= 10000)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);

            flush_queue.push(std::move(memtable));
            memtable.clear();
        }

        flush_cv.notify_one();
    }
}

void TusuEngine::remove(const std::string &key)
{
    uint64_t offset = writeTombstoneRecord(db_file, key);
    memtable[key] = offset;

    if (memtable.size() >= 10000)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);

            flush_queue.push(std::move(memtable));
            memtable.clear();
        }

        flush_cv.notify_one();
    }
}

std::string TusuEngine::get(const std::string &key)
{
    if (memtable.find(key) != memtable.end())
    {
        uint64_t offset = memtable[key];
        std::ifstream infile(db_file, std::ios::binary);
        if (!infile.is_open())
            return "FILE ERROR";

        infile.seekg(offset);
        RecordHeader header;
        infile.read(reinterpret_cast<char *>(&header), sizeof(RecordHeader));
        if (header.is_tombstone != 1)
        {
            infile.seekg(header.keySize, std::ios::cur);

            std::string value(header.valueSize, '\0');
            infile.read(&value[0], header.valueSize);
            return value;
        }

        return "NOT FOUND";
    }

    // 2. Fall back to SSTables (disk search path)
    std::shared_lock lock(sstable_mtx);
    std::string sst_result = getSStable(sstable_files, key, index_cache);
    if (sst_result != "NOT FOUND")
    {
        return sst_result;
    }

    // 3. Not found anywhere
    return "NOT FOUND";
}