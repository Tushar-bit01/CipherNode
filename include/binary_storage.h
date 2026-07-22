#ifndef BINARY_STORAGE_H
#define BINARY_STORAGE_H

#include <string>

uint64_t writeRecord(const std::string &filename, const std::string &key, const std::string &value);

#endif