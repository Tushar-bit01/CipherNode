#ifndef ENGINE_H
#define ENGINE_H

#include <unordered_map>
#include <vector>
#include <string>
#include "binary_storage.h"
#include "binary_record.h"

class TusuEngine
{
private:
    std::string db_file;
    std::unordered_map<std::string, uint64_t> memtable;
    std::unordered_map<std::string, uint64_t> flushing_map;
    std::vector<std::string> sstable_files;
    void flush();

public:
    TusuEngine(const std::string &filename);
    void put(const std::string &key, const std::string &value);
    std::string get(const std::string &key);
};

#endif