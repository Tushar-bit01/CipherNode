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
}

void TusuEngine::flush()
{
    flushing_map = std::move(memtable);
    memtable.clear();
    std::vector<std::string> sorted_keys;
    sorted_keys.reserve(flushing_map.size());
    for (const auto &pair : flushing_map)
    {
        sorted_keys.push_back(pair.first);
    }
    std::sort(sorted_keys.begin(), sorted_keys.end());
    writeSStable(sorted_keys, flushing_map,sstable_files,index_cache);
    checkAndCompactSSTables(sstable_files,index_cache);
}

void TusuEngine::put(const std::string &key, const std::string &value)
{
    uint64_t offset = writeRecord(db_file, key, value);
    memtable[key] = offset;
    if (memtable.size() >= 10000)
        flush();
}

void TusuEngine::remove(const std::string &key)
{
    uint64_t offset = writeTombstoneRecord(db_file, key);
    memtable[key] = offset;
    if (memtable.size() >= 10000)
        flush();
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
        if(header.is_tombstone!=1){
            infile.seekg(header.keySize, std::ios::cur);
        
            std::string value(header.valueSize, '\0');
            infile.read(&value[0], header.valueSize);
            return value;
        }

        return "NOT FOUND";
    }

    // 2. Fall back to SSTables (disk search path)
    std::string sst_result = getSStable(sstable_files, key,index_cache);
    if (sst_result != "NOT FOUND")
    {
        return sst_result;
    }

    // 3. Not found anywhere
    return "NOT FOUND";
}