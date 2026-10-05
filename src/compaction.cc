#include "../include/sstable.h"
#include "../include/binary_storage.h"
#include <iostream>
#include <algorithm>

CompactionResult checkAndCompactSSTables(std::vector<SSTable> &sstables)
{
    if (sstables.size() < 8)
        return {false, {}};

    std::unordered_map<std::string, std::pair<std::string, uint8_t>> MergedRecord;

    for (size_t i = 0; i < 8; ++i)
    {
        const auto &sst = sstables[i];
        std::ifstream file(sst.filename, std::ios::binary);

        if (!file.is_open())
            return {false, {}};

        const auto &index_block = sst.index;

        for (const auto &entry : index_block)
        {
            file.seekg(entry.file_offset);

            RecordHeader header{};

            file.read(
                reinterpret_cast<char *>(&header),
                sizeof(RecordHeader));

            file.seekg(header.keySize, std::ios::cur);

            std::string value(header.valueSize, '\0');

            if (header.valueSize > 0)
                file.read(&value[0], header.valueSize);

            MergedRecord[entry.key] = {
                value,
                header.is_tombstone};
        }
    }

    std::vector<std::pair<std::string, std::string>> sorted_records;
    sorted_records.reserve(MergedRecord.size());

    for (const auto &[key, data] : MergedRecord)
    {
        if (data.second == 0)
            sorted_records.push_back({key, data.first});
    }

    std::sort(sorted_records.begin(), sorted_records.end());
    SSTable sst{};
    sst.filename = generateSStable();

    // Temporary WRITE fd
    int write_fd = open(
        sst.filename.c_str(),
        O_WRONLY | O_APPEND | O_CREAT,
        0644);

    if (write_fd == -1)
    {
        std::cerr << "Failed to create compacted SSTable: "
                  << sst.filename << '\n';

        return {false, {}};
    }

    for (const auto &[key, value] : sorted_records)
    {
        uint64_t offset = writeRecord(
            write_fd,
            key,
            value);

        sst.index.push_back({key, offset});
    }

    close(write_fd);

    std::ofstream sst_outfile(
        sst.filename,
        std::ios::binary | std::ios::app);

    if (!sst_outfile.is_open())
        return {false, {}};

    writeIndexBlock(sst_outfile, sst.index);

    sst_outfile.close();

    int fd = open(sst.filename.c_str(), O_RDONLY);

    if (fd == -1)
    {
        std::cerr << "Failed to open compacted SSTable: "
                  << sst.filename << '\n';

        return {false, {}};
    }
    sst.fd = fd;

    return {true, std::move(sst)};
}