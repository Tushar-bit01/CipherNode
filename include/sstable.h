#ifndef SSTABLE_H
#define SSTABLE_H
#include <vector>
#include <string>
#include <unordered_map>
#include "binary_record.h"
#include <fstream>

struct IndexEntry
{
    std::string key;
    uint64_t file_offset;
};

struct SSTableResult
{
    std::string filename;
    std::vector<IndexEntry> index;
};

struct CompactionResult
{
    bool compacted;
    std::string filename;
    std::vector<IndexEntry> index;
    std::vector<std::string> old_files;
};

std::string generateSStable();
void writeIndexBlock(std::ofstream &sst_outfile, std::vector<IndexEntry> &index_block);
std::vector<IndexEntry> readIndexBlock(std::ifstream &file);
SSTableResult writeSStable(std::vector<std::string> &keys, std::unordered_map<std::string, uint64_t> &flushing_map);
std::pair<std::string, uint8_t> readSStable(std::string &key, std::unordered_map<std::string, uint64_t> &flushing_map);
uint64_t writeSStableRecord(const std::string &filename, const std::string &key, const std::string &value, uint8_t is_tombstone);
std::string getSStable(std::vector<std::string> &sstable_files, const std::string &key,std::unordered_map<std::string, std::vector<IndexEntry>> &index_cache);
CompactionResult checkAndCompactSSTables(std::vector<std::string> &sstable_files,std::unordered_map<std::string, std::vector<IndexEntry>> &index_cache);
#endif