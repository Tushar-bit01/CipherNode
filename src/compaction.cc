#include "../include/sstable.h"
#include "./binary_storage.h"
#include <iostream>

void checkAndCompactSSTables(std::vector<std::string> &sstable_files)
{
    // tc: O(N+klogk)
    if (sstable_files.size() < 4)
    {
        return;
    }
    std::vector<std::string> target_files(sstable_files.begin(), sstable_files.begin() + 4);
    std::unordered_map<std::string, std::pair<std::string, uint8_t>> MergedRecord;

    for (const auto &filename : target_files)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open())
            continue;
        auto index_block = readIndexBlock(file);
        for (auto &entry : index_block)
        {
            file.seekg(entry.file_offset);
            RecordHeader header;
            file.read(reinterpret_cast<char *>(&header), sizeof(RecordHeader));
            file.seekg(header.keySize, std::ios::cur);
            std::string value(header.valueSize, '\0');
            if (header.valueSize > 0)
            {
                file.read(&value[0], header.valueSize);
            }
            MergedRecord[entry.key] = {value, header.is_tombstone};
        }
        file.close();
    }
    // 3. Garbage Collection / Tombstone Check & Vector Conversion
    std::vector<std::pair<std::string, std::string>> sorted_records;
    sorted_records.reserve(MergedRecord.size());
    for (auto &[key, data] : MergedRecord)
    {
        if (data.second == 0)
        {
            sorted_records.push_back({key, data.first});
        }
    }

    std::sort(sorted_records.begin(), sorted_records.end());

    std::string sst_filename = generateSStable();
    std::vector<IndexEntry> index_block;
    for (int i = 0; i < sorted_records.size(); i++)
    {
        // writeRecord because all tombstone updated or deleted
        uint64_t offset = writeRecord(sst_filename, sorted_records[i].first, sorted_records[i].second);
        index_block.push_back({sorted_records[i].first, offset});
    }
    std::ofstream sst_outfile(sst_filename, std::ios::binary | std::ios::app);
    if (sst_outfile.is_open())
    {
        writeIndexBlock(sst_outfile, index_block);
        sst_outfile.close();
    }
    // safe atomic replacement
    for (const auto &filename : target_files)
    {
        std::remove(filename.c_str());
        // Find and remove safely from sstable_files vector
        auto it = std::find(sstable_files.begin(), sstable_files.end(), filename);
        if (it != sstable_files.end())
        {
            sstable_files.erase(it);
        }
    }

    // Add the new compacted file to the engine
    sstable_files.push_back(sst_filename);
}
