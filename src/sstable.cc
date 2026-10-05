#include "../include/sstable.h"
#include "../include/binary_storage.h"
#include <iostream>
#include <algorithm>
#include <ctime>

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

SSTable writeSStable(std::vector<std::string> &keys, std::unordered_map<std::string, uint64_t> &flushing_map)
{
    SSTable sst;
    sst.filename = generateSStable();
    std::vector<IndexEntry> index_block;
    for (int i = 0; i < keys.size(); i++)
    {
        auto record = readSStable(keys[i], flushing_map);
        if (record.first == "data not found" || record.first == "WAL file not found")
            continue;
        uint64_t record_offset = writeSStableRecord(sst.filename, keys[i], record.first, record.second);
        index_block.push_back({keys[i], record_offset});
    }
    std::ofstream sst_outfile(sst.filename, std::ios::binary | std::ios::app);
    if (sst_outfile.is_open())
    {
        writeIndexBlock(sst_outfile, index_block);
        sst_outfile.close();
    }
    sst.index = std::move(index_block);
    int fd = open(sst.filename.c_str(), O_RDONLY);
    if (fd == -1)
    {
        std::cerr << "Failed to open SSTable: "
                  << sst.filename << '\n';
    }
    else
    {
        sst.fd = fd;
    }
    return sst;
}

std::string getSStable(
    std::vector<SSTable> &sstables,
    const std::string &key)
{
    // 1. Loop through SSTable files from newest to oldest
    int size = sstables.size();
    for (int i = size - 1; i >= 0; i--)
    {
        const auto &sst = sstables[i];
        const auto &index_block = sst.index;

        // 6. Run Binary Search on the index_block vector
        int low = 0;
        int high = static_cast<int>(index_block.size()) - 1;
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
            RecordHeader header{};
            //read from found offset to only keysize valuesize to actually read value and key data 
            //we have to add found_offset+sizeof(RecordHeader)+header.keySize to read value data or just recordheader to read key
            ssize_t bytes_read = pread(
                sst.fd,
                &header,
                sizeof(RecordHeader),
                found_offset);

            if (bytes_read != sizeof(RecordHeader))
                continue;

            if (header.is_tombstone == 1)
                return "NOT FOUND";

            std::string value(header.valueSize, '\0');

            bytes_read = pread(
                sst.fd,
                value.data(),
                header.valueSize,
                found_offset + sizeof(RecordHeader) + header.keySize);

            if (bytes_read != header.valueSize)
                continue;

            return value;
        }
    }

    return "NOT FOUND";
}