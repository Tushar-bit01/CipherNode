#ifndef SSTABLE_H
#define SSTABLE_H
#include <vector>
#include <string>
#include <unordered_map>
#include "binary_record.h"
#include <fstream>
#include <fcntl.h>
#include <unistd.h>

struct IndexEntry
{
    std::string key;
    uint64_t file_offset;
};

struct SSTable
{
    std::string filename;
    int fd=-1;
    std::vector<IndexEntry> index;
};

struct CompactionResult
{
    bool compacted;
    SSTable new_sstable;
};



std::string generateSStable();
void writeIndexBlock(std::ofstream &sst_outfile, std::vector<IndexEntry> &index_block);
std::vector<IndexEntry> readIndexBlock(std::ifstream &file);
SSTable writeSStable(std::vector<std::string> &keys, std::unordered_map<std::string, uint64_t> &flushing_map);
std::pair<std::string, uint8_t> readSStable(std::string &key, std::unordered_map<std::string, uint64_t> &flushing_map);
uint64_t writeSStableRecord(const std::string &filename, const std::string &key, const std::string &value, uint8_t is_tombstone);
std::string getSStable(
    std::vector<SSTable> &sstables,
    const std::string &key);
CompactionResult checkAndCompactSSTables(std::vector<SSTable> &sstables);
std::string readRecord(int fd, uint64_t offset);

#endif