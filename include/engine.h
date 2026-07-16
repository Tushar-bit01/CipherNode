#ifndef ENGINE_H
#define ENGINE_H

#include <unordered_map>
#include <string>
#include "binary_storage.h"

class TusuEngine {
private:
    std::string db_file;
    std::unordered_map<std::string, uint64_t> memtable; 

public:
    TusuEngine(const std::string &filename);
    void put(const std::string &key, const std::string &value);
    std::string get(const std::string &key);
};

#endif