#ifndef SSTABLE_H
#define SSTABLE_H
#include <vector>
#include <string>
#include <unordered_map>
#include "binary_record.h"

void writeSStable(std::vector<std::string> &keys, std::unordered_map<std::string, uint64_t> &flushing_map,std::vector<std::string> &sstable_files);
std::string readSStable(std::string key, std::unordered_map<std::string, uint64_t> &flushing_map);
std::string getSStable(std::vector<std::string> &sstable_files,const std::string &key);
#endif