#include "../include/sstable.h"
#include "../include/binary_storage.h"
#include <iostream>
#include <algorithm>

void checkAndCompactSSTables(
    std::vector<std::string> &sstable_files,
    std::unordered_map<std::string, std::vector<IndexEntry>> &index_cache)
{
    if (sstable_files.size() < 8)
        return;

    std::vector<std::string> target_files(
        sstable_files.begin(),
        sstable_files.begin() + 8);

    std::unordered_map<std::string, std::pair<std::string, uint8_t>> MergedRecord;

    for (const auto &filename : target_files)
    {
        std::ifstream file(filename, std::ios::binary);

        if (!file.is_open())
            return;

        auto it = index_cache.find(filename);

        // If index is not cached, load it from disk and cache it.
        if (it == index_cache.end())
        {
            index_cache[filename] = readIndexBlock(file);
            it = index_cache.find(filename);
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
                header.is_tombstone
            };
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
        return;

    writeIndexBlock(sst_outfile, index_block);

    // Cache the new SSTable index.
    index_cache[sst_filename] = std::move(index_block);

    sst_outfile.close();

    // Remove old SSTables and their cache entries.
    for (const auto &filename : target_files)
    {
        std::remove(filename.c_str());
        index_cache.erase(filename);
    }

    // Remove the first 8 old SSTables.
    sstable_files.erase(
        sstable_files.begin(),
        sstable_files.begin() + 8);

    // Compacted SSTable represents the old range,
    // so keep it before newer SSTables.
    sstable_files.insert(
        sstable_files.begin(),
        sst_filename);
}