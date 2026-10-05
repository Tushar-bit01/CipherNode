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

    wal_fd = open(db_file.c_str(), O_RDONLY | O_CREAT, 0644);
    if (wal_fd == -1)
    {
        throw std::runtime_error("Failed to open WAL for reading");
    }

    wal_write_fd = open(
        db_file.c_str(),
        O_WRONLY | O_APPEND | O_CREAT,
        0644);

    if (wal_write_fd == -1)
    {
        close(wal_fd);
        throw std::runtime_error("Failed to open WAL for writing");
    }

    // Discover existing SSTables
    for (const auto &entry : std::filesystem::directory_iterator("."))
    {
        std::string filename = entry.path().filename().string();

        if (filename.rfind("sstable_", 0) == 0 &&
            entry.path().extension() == ".db")
        {
            int fd = open(filename.c_str(), O_RDONLY);
            if (fd != -1)
            {
                std::ifstream file(filename, std::ios::binary);
                if (file.is_open())
                {
                    SSTable sst;
                    sst.filename = filename;
                    sst.fd = fd;
                    sst.index = readIndexBlock(file);
                    sstables.push_back(std::move(sst));
                }
                else
                {
                    close(fd);
                }
            }
        }
    }
    std::sort(sstables.begin(), sstables.end(),
              [](const SSTable &a, const SSTable &b)
              {
                  return a.filename < b.filename;
              });

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
    for (auto &sst : sstables)
    {
        if (sst.fd != -1)
            close(sst.fd);
    }
    close(wal_fd);
    close(wal_write_fd);
}

void TusuEngine::flush(Batch &batch)
{
    std::vector<std::string> sorted_keys;
    sorted_keys.reserve(batch.size());
    for (const auto &pair : batch)
    {
        sorted_keys.push_back(pair.first);
    }
    std::sort(sorted_keys.begin(), sorted_keys.end());
    SSTable sst = writeSStable(sorted_keys, batch);

    {
        std::unique_lock lock(sstable_mtx);

        sstables.push_back(std::move(sst));
    }

    CompactionResult compacted = checkAndCompactSSTables(sstables);
    if (compacted.compacted)
    {
        std::unique_lock lock(sstable_mtx);
        // Remove old SSTables
        for (size_t i = 0; i < 8; ++i)
        {
            auto &sst = sstables[i];
            close(sst.fd);
            std::remove(sst.filename.c_str());
        }

        // Remove the first 8 old SSTables.
        sstables.erase(
            sstables.begin(),
            sstables.begin() + 8);
        // Compacted SSTable represents the old range,
        // so keep it before newer SSTables.
        sstables.insert(
            sstables.begin(),
            std::move(compacted.new_sstable));
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

            flushing_batch = std::move(flush_queue.front());
            flush_queue.pop_front();
        }
        flush(*flushing_batch);
        {
            std::lock_guard<std::mutex> lock(mtx);
            flushing_batch.reset();
        }
    }
}

void TusuEngine::put(const std::string &key, const std::string &value)
{
    uint64_t offset = writeRecord(wal_write_fd, key, value);
    memtable[key] = offset;
    if (memtable.size() >= 10000)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);

            flush_queue.push_back(std::move(memtable));
            memtable.clear();
        }

        flush_cv.notify_one();
    }
}

void TusuEngine::remove(const std::string &key)
{
    uint64_t offset = writeTombstoneRecord(wal_write_fd, key);
    memtable[key] = offset;

    if (memtable.size() >= 10000)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);

            flush_queue.push_back(std::move(memtable));
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
        return readRecord(wal_fd, offset);
    }

    uint64_t offset;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(mtx);

        for (auto it = flush_queue.rbegin();
             it != flush_queue.rend();
             ++it)
        {
            auto batch_it = it->find(key);

            if (batch_it != it->end())
            {
                offset = batch_it->second;
                found = true;
                break;
            }
        }

        if (!found && flushing_batch)
        {
            auto it = flushing_batch->find(key);

            if (it != flushing_batch->end())
            {
                offset = it->second;
                found = true;
            }
        }
    }

    if (found)
    {
        return readRecord(wal_fd, offset);
    }

    // 2. Fall back to SSTables (disk search path)
    std::shared_lock lock(sstable_mtx);
    std::string sst_result = getSStable(sstables, key);
    if (sst_result != "NOT FOUND")
    {
        return sst_result;
    }

    // 3. Not found anywhere
    return "NOT FOUND";
}