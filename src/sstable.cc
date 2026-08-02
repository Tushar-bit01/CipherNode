#include "../include/sstable.h"
#include "./binary_storage.h"
#include<unordered_map>
#include <fstream>
#include <iostream>


void checkAndCompactSSTables(std::vector<std::string> &sstable_files) {
    //tc: O(N+klogk)
    if (sstable_files.size() < 4) {
        return;
    }
    std::vector<std::string> target_files(sstable_files.begin(), sstable_files.begin() + 4);
    std::unordered_map<std::string,std::pair<std::string,uint8_t>> MergedRecord;

    for(const auto &filename : target_files){
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) continue;
        file.seekg(-static_cast<int>(sizeof(uint64_t)),std::ios::end);
        uint64_t index_start_position=0;
        file.read(reinterpret_cast<char*>(&index_start_position),sizeof(uint64_t));
        file.seekg(index_start_position);
        size_t total_keys = 0;
        file.read(reinterpret_cast<char *>(&total_keys), sizeof(size_t));
        std::vector<IndexEntry> index_entries;
        index_entries.reserve(total_keys);
        for(int j=0;j<total_keys;j++){
            uint32_t key_length = 0;
            file.read(reinterpret_cast<char *>(&key_length), sizeof(uint32_t));
            std::string key(key_length, '\0');
            file.read(&key[0], key_length);
            uint64_t offset=0;
            file.read(reinterpret_cast<char *>(&offset), sizeof(uint64_t));
            index_entries.push_back({key, offset});
        }
        for(auto &entry:index_entries){
            file.seekg(entry.file_offset);
            RecordHeader header;
            file.read(reinterpret_cast<char *>(&header), sizeof(RecordHeader));
            file.seekg(header.keySize,std::ios::cur);
            std::string value(header.valueSize, '\0');
            if (header.valueSize > 0) {
                file.read(&value[0], header.valueSize);
            }
            MergedRecord[entry.key]={value,header.is_tombstone};
        }
        file.close();
    }
    // 3. Garbage Collection / Tombstone Check & Vector Conversion
    std::cout << "--- Keys currently inside MergedRecord during compaction ---\n";
    for (const auto &[k, v] : MergedRecord) {
        std::cout << "Key: " << k << ", Tombstone: " << (int)v.second << ", Value: " << v.first << "\n";
    }
    std::vector<std::pair<std::string, std::string>> sorted_records;
    sorted_records.reserve(MergedRecord.size());
    for(auto &[key,data]:MergedRecord){
        if (data.second == 0) {
            sorted_records.push_back({key, data.first});
        }
    }

    std::sort(sorted_records.begin(), sorted_records.end());

    std::vector<std::string> old_sstables = sstable_files;
    static int sst_counter = 0;
    std::string sst_filename = "sstable_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(sst_counter++) + ".db";
    RecordHeader header;
    std::vector<IndexEntry> index_block;
    for(int i=0;i<sorted_records.size();i++){
        uint64_t offset=writeRecord(sst_filename,sorted_records[i].first,sorted_records[i].second);
        index_block.push_back({sorted_records[i].first,offset});
    }
    std::ofstream sst_outfile(sst_filename, std::ios::binary | std::ios::app);
    if (sst_outfile.is_open())
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
        sst_outfile.write(reinterpret_cast<const char*>(&index_start_position), sizeof(uint64_t));
        sst_outfile.close();
        //safe atomic replacement
        for (const auto &filename : target_files) {
            std::remove(filename.c_str());
            // Find and remove safely from sstable_files vector
            auto it = std::find(sstable_files.begin(), sstable_files.end(), filename);
            if (it != sstable_files.end()) {
                sstable_files.erase(it);
            }
        }
        
        // Add the new compacted file to the engine
        sstable_files.push_back(sst_filename);
    }
}

std::string readSStable(std::string key, std::unordered_map<std::string, uint64_t> &flushing_map)
{
    std::ifstream wal_file("tusu.db", std::ios::binary);
    if (!wal_file.is_open())
    {
        return "WAL file not found";
    }
    if (flushing_map.find(key) == flushing_map.end())
    {
        return "data not found";
    }
    uint64_t offset = flushing_map[key];
    RecordHeader header;
    wal_file.seekg(offset);
    wal_file.read(reinterpret_cast<char *>(&header), sizeof(RecordHeader));
    wal_file.seekg(header.keySize, std::ios::cur);
    std::string value(header.valueSize, '\0');
    wal_file.read(&value[0], header.valueSize);
    return value;
}

void writeSStable(std::vector<std::string> &keys, std::unordered_map<std::string, uint64_t> &flushing_map, std::vector<std::string> &sstable_files)
{
    // 1. Generate a unique SSTable filename (e.g., using current timestamp or a counter)
    static int sst_counter = 0;
    std::string sst_filename = "sstable_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(sst_counter++) + ".db";   
    sstable_files.push_back(sst_filename);
    RecordHeader header;
    std::vector<IndexEntry> index_block;
    for (int i = 0; i < keys.size(); i++)
    {
        std::string value = readSStable(keys[i], flushing_map);
        if (value == "data not found" || value == "WAL file not found")
            continue;
        uint64_t record_offset = writeRecord(sst_filename, keys[i], value);
        index_block.push_back({keys[i], record_offset});
    }
    std::ofstream sst_outfile(sst_filename, std::ios::binary | std::ios::app);
    if (sst_outfile.is_open())
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
        sst_outfile.write(reinterpret_cast<const char*>(&index_start_position), sizeof(uint64_t));
        sst_outfile.close();
    }
    flushing_map.clear();
}

std::string getSStable(std::vector<std::string> &sstable_files, const std::string &key) {
    // 1. Loop through SSTable files from newest to oldest
    for (int i = sstable_files.size() - 1; i >= 0; i--) {
        std::ifstream file(sstable_files[i], std::ios::binary);
        if (!file.is_open()) continue;

        // 2. Read the Footer (last 8 bytes) to find where the index block starts
        file.seekg(-static_cast<int>(sizeof(uint64_t)), std::ios::end);
        uint64_t index_start_position = 0;
        file.read(reinterpret_cast<char*>(&index_start_position), sizeof(uint64_t));

        // 3. Jump to the index block position
        file.seekg(index_start_position);

        // 4. Read total keys in the index block
        size_t total_keys = 0;
        file.read(reinterpret_cast<char*>(&total_keys), sizeof(size_t));

        // 5. Read all index entries into a local vector
        std::vector<IndexEntry> index_block;
        for (size_t j = 0; j < total_keys; j++) {
            uint32_t key_length = 0;
            file.read(reinterpret_cast<char*>(&key_length), sizeof(uint32_t));

            std::string current_key(key_length, '\0');
            file.read(&current_key[0], key_length);

            uint64_t record_offset = 0;
            file.read(reinterpret_cast<char*>(&record_offset), sizeof(uint64_t));

            index_block.push_back({current_key, record_offset});
        }

        // 6. Run Binary Search on the index_block vector
        int low = 0;
        int high = index_block.size() - 1;
        uint64_t found_offset = 0;
        bool found = false;

        while (low <= high) {
            int mid = low + (high - low) / 2;
            if (index_block[mid].key == key) {
                found_offset = index_block[mid].file_offset;
                found = true;
                break;
            } else if (index_block[mid].key < key) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }

        // 7. If found via binary search, jump to the record and read the value!
        if (found) {
            file.seekg(found_offset);
            RecordHeader header;
            file.read(reinterpret_cast<char*>(&header), sizeof(RecordHeader));
            if(header.is_tombstone==1){
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