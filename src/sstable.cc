#include "../include/sstable.h"
#include "./binary_storage.h"
#include <iostream>

static int sst_counter = 0;

std::string generateSStable()
{
    return "sstable_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(sst_counter++) + ".db";
}

void writeIndexBlock(
    std::ofstream &sst_outfile,
    std::vector<IndexEntry> &index_block)
{
    uint64_t index_start_position = sst_outfile.tellp();
    size_t total_keys = index_block.size();
    sst_outfile.write(reinterpret_cast<const char *>(&total_keys), sizeof(size_t));
    for (int i = 0; i < index_block.size(); i++)
    {
        std::string current_key = index_block[i].key;
        uint64_t offset = index_block[i].file_offset;
        uint32_t key_length = static_cast<uint32_t>(current_key.size());
        sst_outfile.write(reinterpret_cast<const char *>(&key_length), sizeof(uint32_t));
        sst_outfile.write(current_key.data(), key_length);
        sst_outfile.write(reinterpret_cast<const char *>(&offset), sizeof(uint64_t));
    }
    sst_outfile.write(reinterpret_cast<const char *>(&index_start_position), sizeof(uint64_t));
}

std::vector<IndexEntry> readIndexBlock(std::ifstream &file)
{
    file.seekg(-static_cast<int>(sizeof(uint64_t)), std::ios::end);

    uint64_t index_start_position = 0;
    file.read(reinterpret_cast<char *>(&index_start_position), sizeof(uint64_t));

    file.seekg(index_start_position);

    size_t total_keys = 0;
    file.read(reinterpret_cast<char *>(&total_keys), sizeof(size_t));

    std::vector<IndexEntry> index_block;
    index_block.reserve(total_keys);

    for (size_t i = 0; i < total_keys; i++)
    {
        uint32_t key_length = 0;
        file.read(reinterpret_cast<char *>(&key_length), sizeof(uint32_t));

        std::string key(key_length, '\0');
        file.read(&key[0], key_length);

        uint64_t offset = 0;
        file.read(reinterpret_cast<char *>(&offset), sizeof(uint64_t));

        index_block.push_back({key, offset});
    }

    return index_block;
}

uint64_t writeSStableRecord(const std::string &filename, const std::string &key, const std::string &value, uint8_t is_tombstone)
{
    std::ofstream outfile(filename, std::ios::binary | std::ios::app);
    if (!outfile.is_open())
    {
        return 0;
    }
    uint64_t offset = outfile.tellp();
    RecordHeader header{};
    header.keySize = static_cast<uint32_t>(key.size());
    header.valueSize = static_cast<uint32_t>(value.size());
    header.is_tombstone = is_tombstone;
    outfile.write(reinterpret_cast<const char *>(&header), sizeof(RecordHeader));
    outfile.write(key.data(), header.keySize);
    outfile.write(value.data(), header.valueSize);
    outfile.close();
    return offset;
}

std::pair<std::string, uint8_t> readSStable(std::string &key, std::unordered_map<std::string, uint64_t> &flushing_map)
{
    std::ifstream wal_file("tusu.db", std::ios::binary);
    if (!wal_file.is_open())
    {
        return {"WAL file not found", 0};
    }
    if (flushing_map.find(key) == flushing_map.end())
    {
        return {"data not found", 0};
    }
    uint64_t offset = flushing_map[key];
    RecordHeader header{};
    wal_file.seekg(offset);
    wal_file.read(reinterpret_cast<char *>(&header), sizeof(RecordHeader));
    wal_file.seekg(header.keySize, std::ios::cur);
    std::string value(header.valueSize, '\0');
    wal_file.read(&value[0], header.valueSize);
    return {value, header.is_tombstone};
}

void writeSStable(std::vector<std::string> &keys, std::unordered_map<std::string, uint64_t> &flushing_map, std::vector<std::string> &sstable_files)
{
    // 1. Generate a unique SSTable filename (e.g., using current timestamp or a counter)
    std::string sst_filename = generateSStable();
    sstable_files.push_back(sst_filename);
    std::vector<IndexEntry> index_block;
    for (int i = 0; i < keys.size(); i++)
    {
        auto record = readSStable(keys[i], flushing_map);
        if (record.first == "data not found" || record.first == "WAL file not found")
            continue;
        uint64_t record_offset = writeSStableRecord(sst_filename, keys[i], record.first, record.second);
        index_block.push_back({keys[i], record_offset});
    }
    std::ofstream sst_outfile(sst_filename, std::ios::binary | std::ios::app);
    if (sst_outfile.is_open())
    {
        writeIndexBlock(sst_outfile, index_block);
        sst_outfile.close();
    }
    flushing_map.clear();
}

std::string getSStable(std::vector<std::string> &sstable_files, const std::string &key)
{
    // 1. Loop through SSTable files from newest to oldest
    for (int i = sstable_files.size() - 1; i >= 0; i--)
    {
        std::ifstream file(sstable_files[i], std::ios::binary);
        if (!file.is_open())
            continue;
        auto index_block = readIndexBlock(file);

        // 6. Run Binary Search on the index_block vector
        int low = 0;
        int high = index_block.size() - 1;
        uint64_t found_offset = 0;
        bool found = false;

        while (low <= high)
        {
            int mid = low + (high - low) / 2;
            if (index_block[mid].key == key)
            {
                found_offset = index_block[mid].file_offset;
                found = true;
                break;
            }
            else if (index_block[mid].key < key)
            {
                low = mid + 1;
            }
            else
            {
                high = mid - 1;
            }
        }

        // 7. If found via binary search, jump to the record and read the value!
        if (found)
        {
            file.seekg(found_offset);
            RecordHeader header;
            file.read(reinterpret_cast<char *>(&header), sizeof(RecordHeader));
            if (header.is_tombstone == 1)
            {
                return "NOT FOUND";
            }
            // Skip past the key to reach the value
            file.seekg(header.keySize, std::ios::cur);

            std::string value(header.valueSize, '\0');
            file.read(&value[0], header.valueSize);

            file.close();
            return value;
        }

        file.close();
    }

    return "NOT FOUND";
}