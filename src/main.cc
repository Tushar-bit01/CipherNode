#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include "../include/binary_storage.h"
#include "../include/sstable.h"

int main() {
    std::cout << "=== Starting Database & SSTable Engine Tests ===" << std::endl;

    // State trackers for your engine
    std::unordered_map<std::string, uint64_t> flushing_map;
    std::vector<std::string> sstable_files;

    // 1. Simulate writing records (this mimics your WAL + MemTable tracking)
    std::cout << "\n[Test 1] Writing records to WAL..." << std::endl;
    
    // Let's write a few sorted or unsorted keys
    std::vector<std::string> test_keys = {"apple", "banana", "cat", "dog", "elephant"};
    std::vector<std::string> test_values = {"red fruit", "yellow fruit", "feline", "canine", "mammal"};

    for (size_t i = 0; i < test_keys.size(); i++) {
        uint64_t offset = writeRecord("tusu.db", test_keys[i], test_values[i]);
        flushing_map[test_keys[i]] = offset;
        std::cout << "Written key: " << test_keys[i] << " at WAL offset: " << offset << std::endl;
    }

    // 2. Simulate a Memtable Flush -> Creating an SSTable
    std::cout << "\n[Test 2] Flushing MemTable to SSTable..." << std::endl;
    
    // Sort keys before passing to writeSStable (SSTables require sorted keys!)
    std::vector<std::string> keys_to_flush = {"apple", "banana", "cat", "dog", "elephant"};
    
    writeSStable(keys_to_flush, flushing_map, sstable_files);
    
    if (!sstable_files.empty()) {
        std::cout << "Successfully generated SSTable file: " << sstable_files[0] << std::endl;
    } else {
        std::cout << "Error: SSTable file was not generated." << std::endl;
        return 1;
    }

    // 3. Test Reading via SSTable Binary Search (`getSStable`)
    std::cout << "\n[Test 3] Testing Binary Search Read (`getSStable`)...\n" << std::endl;

    // Query an existing key
    std::string query_key = "cat";
    std::string result = getSStable(sstable_files, query_key);
    std::cout << "Querying key '" << query_key << "' -> Result: " << result << " (Expected: feline)" << std::endl;

    // Query another existing key
    query_key = "apple";
    result = getSStable(sstable_files, query_key);
    std::cout << "Querying key '" << query_key << "' -> Result: " << result << " (Expected: red fruit)" << std::endl;

    // Query a non-existent key
    query_key = "zebra";
    result = getSStable(sstable_files, query_key);
    std::cout << "Querying key '" << query_key << "' -> Result: " << result << " (Expected: NOT FOUND)" << std::endl;

    std::cout << "\n=== All Tests Completed Successfully! ===" << std::endl;
    return 0;
}