#include "../include/sstable.h"
#include "../include/binary_storage.h"
#include <iostream>
#include <algorithm>

CompactionResult checkAndCompactSSTables(
    std::vector<std::string> &sstable_files,
    std::unordered_map<std::string, std::vector<IndexEntry>> &index_cache)
{
    if (sstable_files.size() < 8)
        return {false, "", {}, {}};

    std::vector<std::string> target_files(
        sstable_files.begin(),
        sstable_files.begin() + 8);

    std::unordered_map<std::string, std::pair<std::string, uint8_t>> MergedRecord;

    for (const auto &filename : target_files)
    {
        std::ifstream file(filename, std::ios::binary);

        if (!file.is_open())
            return {false, "", {}, {}};

        auto it = index_cache.find(filename);

        if (it == index_cache.end())
        {
            return {false, "", {}, {}};
        }

        const auto &index_block = it->second;

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

    std::string sst_filename = generateSStable();

    std::vector<IndexEntry> index_block;
    index_block.reserve(sorted_records.size());

    for (const auto &[key, value] : sorted_records)
    {
        uint64_t offset = writeRecord(
            sst_filename,
            key,
            value);

        index_block.push_back({key, offset});
    }

    std::ofstream sst_outfile(
        sst_filename,
        std::ios::binary | std::ios::app);

    if (!sst_outfile.is_open())
        return {false, "", {}, {}};

    writeIndexBlock(sst_outfile, index_block);

    sst_outfile.close();

    return {true,std::move(sst_filename) , std::move(index_block), std::move(target_files)};
}