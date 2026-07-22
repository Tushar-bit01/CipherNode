#include "../include/engine.h"
#include "../include/sstable.h"
#include <fstream>
#include <iostream>
#include <vector>

TusuEngine::TusuEngine(const std::string &filename) : db_file(filename)
{
    std::ifstream infile(db_file, std::ios::binary);
    if (!infile.is_open())
        return;

    RecordHeader header;

    while (true)
    {
        uint64_t current_offset = infile.tellg();
        if (!infile.read(reinterpret_cast<char *>(&header), sizeof(RecordHeader)))
            break;
        std::string key(header.keySize, '\0');
        infile.read(&key[0], header.keySize);
        infile.seekg(header.valueSize, std::ios::cur);
        memtable[key] = current_offset;
    }
};

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
    writeSStable(sorted_keys, flushing_map,sstable_files);
}

void TusuEngine::put(const std::string &key, const std::string &value)
{
    uint64_t offset = writeRecord(db_file, key, value);
    memtable[key] = offset;
    if (memtable.size() > 1000)
        flush();
}

std::string TusuEngine::get(const std::string &key)
{
    if (memtable.find(key) == memtable.end())
    {
        return "NOT FOUND";
    }
    auto it = memtable.find(key);
    uint64_t offset = it->second;

    std::ifstream infile(db_file, std::ios::binary);
    if (!infile.is_open())
        return "FILE ERROR";
    infile.seekg(offset);

    RecordHeader header;
    infile.read(reinterpret_cast<char *>(&header), sizeof(RecordHeader));
    // skip keysize bytes
    infile.seekg(header.keySize, std::ios::cur);
    // make buffer of size value
    std::string value(header.valueSize, '\0');
    infile.read(&value[0], header.valueSize);

    return value;
}