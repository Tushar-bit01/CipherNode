#ifndef ENGINE_H
#define ENGINE_H

#include <unordered_map>
#include <vector>
#include <string>
#include "binary_storage.h"
#include "binary_record.h"
#include "sstable.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <optional>
#include <shared_mutex>
#include <fcntl.h>
#include <unistd.h>

class TusuEngine
{
private:
    int wal_fd = -1;
    int wal_write_fd = -1; 
    std::string db_file;
    std::unordered_map<std::string, uint64_t> memtable;
    using Batch = std::unordered_map<std::string, uint64_t>;

    std::deque<Batch> flush_queue;
    std::optional<Batch> flushing_batch;
    std::vector<SSTable> sstables;
    void flush(std::unordered_map<std::string, uint64_t> &batch);
    std::thread flush_thread;
    std::mutex mtx;//flush_queue
    std::condition_variable flush_cv;
    bool shutting_down = false;
    void flushWorker();
    std::shared_mutex sstable_mtx; //sstable+indexcache

public:
    TusuEngine(const std::string &filename);
    ~TusuEngine();
    void put(const std::string &key, const std::string &value);
    void remove(const std::string &key);
    std::string get(const std::string &key);
};

#endif