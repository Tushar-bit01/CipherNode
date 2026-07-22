#include "../include/sstable.h"
#include "./binary_storage.h"
#include <fstream>

std::string readSStable(std::string key, std::unordered_map<std::string, uint64_t> &flushing_map)
{
    std::ifstream wal_file("tusu.db", std::ios::binary);
    if (!wal_file.is_open())
    {
        return "WAL file not found";
    }
    if (flushing_map.find(key) == flushing_map.end())
    {
        return "data not found";
    }
    uint64_t offset = flushing_map[key];
    RecordHeader header;
    wal_file.seekg(offset);
    wal_file.read(reinterpret_cast<char *>(&header), sizeof(RecordHeader));
    wal_file.seekg(header.keySize, std::ios::cur);
    std::string value(header.valueSize, '\0');
    wal_file.read(&value[0], header.valueSize);
    return value;
}

void writeSStable(std::vector<std::string> &keys, std::unordered_map<std::string, uint64_t> &flushing_map,std::vector<std::string> &sstable_files)
{
    // 1. Generate a unique SSTable filename (e.g., using current timestamp or a counter)
    std::string sst_filename = "sstable_" + std::to_string(std::time(nullptr)) + ".db";
    sstable_files.push_back(sst_filename);
    RecordHeader header;
    for (int i = 0; i < keys.size(); i++)
    {
        std::string value = readSStable(keys[i], flushing_map);
        if (value == "data not found" || value == "WAL file not found")
            continue;
        writeRecord(sst_filename, keys[i], value);
    }
    flushing_map.clear();
}

std::string getSStable(std::vector<std::string> &sstable_files,const std::string &key){
    //implement next time
}