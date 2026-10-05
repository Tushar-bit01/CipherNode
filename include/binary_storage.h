#ifndef BINARY_STORAGE_H
#define BINARY_STORAGE_H

#include <string>
#include <cstdint>

uint64_t writeRecord(int fd, const std::string &key, const std::string &value);
uint64_t writeTombstoneRecord(int fd,const std::string &key);

#endif