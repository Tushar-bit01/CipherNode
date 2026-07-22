#ifndef BINARY_RECORD_H
#define BINARY_RECORD_H

#include <cstdint>
// memory layout control
#pragma pack(push, 1)

struct RecordHeader
{
    uint32_t keySize;
    uint32_t valueSize;
};

#pragma pack(pop)

#endif