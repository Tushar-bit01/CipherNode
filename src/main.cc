#include <iostream>
#include <cassert>
#include "../include/engine.h"

void runTests() 
{
    // Clean start for test database
    system("rm -f tusu.db sstable_*.db");

    std::cout << "[INFO] Initializing TusuEngine...\n";
    TusuEngine db("tusu.db");

    // -------------------------------------------------------------
    // Test 1: Basic Put and Get
    // -------------------------------------------------------------
    std::cout << "\n--- Test 1: Basic Put & Get ---\n";
    db.put("apple", "red");
    db.put("banana", "yellow");

    assert(db.get("apple") == "red");
    assert(db.get("banana") == "yellow");
    std::cout << "[PASS] Basic Put & Get works!\n";

    // -------------------------------------------------------------
    // Test 2: Updates (Overwriting keys)
    // -------------------------------------------------------------
    std::cout << "\n--- Test 2: Updates ---\n";
    db.put("apple", "green"); // Update value
    assert(db.get("apple") == "green");
    std::cout << "[PASS] Updates work correctly!\n";

    // -------------------------------------------------------------
    // Test 3: Threshold Flush to SSTable (> 3 items triggers flush)
    // -------------------------------------------------------------
    std::cout << "\n--- Test 3: MemTable Flush to SSTable ---\n";
    db.put("cherry", "red");
    db.put("date", "brown"); // This 4th item should trigger flush()!
    
    assert(db.get("apple") == "green");
    assert(db.get("banana") == "yellow");
    assert(db.get("cherry") == "red");
    assert(db.get("date") == "brown");
    std::cout << "[PASS] Flushes and SSTable lookups work!\n";

    // -------------------------------------------------------------
    // Test 4: Tombstones & Deletions
    // -------------------------------------------------------------
    std::cout << "\n--- Test 4: Deletions (Tombstones) ---\n";
    db.remove("banana");
    
    assert(db.get("banana") == "NOT FOUND");
    assert(db.get("apple") == "green"); 
    std::cout << "[PASS] Tombstone deletion works perfectly!\n";

    // -------------------------------------------------------------
    // Test 5: Re-inserting a deleted key (Resurrection)
    // -------------------------------------------------------------
    std::cout << "\n--- Test 5: Re-inserting Deleted Key ---\n";
    db.put("banana", "sweet-yellow"); // Bring it back
    assert(db.get("banana") == "sweet-yellow");
    std::cout << "[PASS] Resurrection of deleted key works!\n";

    // -------------------------------------------------------------
    // Test 6: Compaction Trigger Test (>= 4 SSTable files)
    // -------------------------------------------------------------
    std::cout << "\n--- Test 6: Compaction & Tombstone Garbage Collection ---\n";
    // Let's force multiple flushes to accumulate >= 4 SSTable files
    // Flush 2
    db.put("grape", "purple");
    db.put("fig", "brown");
    db.put("kiwi", "green");
    db.put("lemon", "yellow"); 

    // Flush 3
    db.put("mango", "orange");
    db.put("nectarine", "pink");
    db.put("orange", "orange");
    db.put("papaya", "green"); 

    // Flush 4 (This should cross the threshold of 4 SSTable files and trigger compaction!)
    db.put("quince", "yellow");
    db.put("raspberry", "red");
    db.put("strawberry", "red");
    db.put("tomato", "red"); 

    // Verify all existing keys can still be fetched post-compaction
    std::cout << "[DEBUG] apple value is: '" << db.get("apple") << "'\n";
    assert(db.get("apple") == "green");
    assert(db.get("banana") == "sweet-yellow");
    assert(db.get("cherry") == "red");
    assert(db.get("date") == "brown");
    assert(db.get("mango") == "orange");
    assert(db.get("strawberry") == "red");
    std::cout << "[PASS] Compaction executed and all data remains fully intact!\n";

    // -------------------------------------------------------------
    // Test 7: Persistence / Recovery from WAL on Restart
    // -------------------------------------------------------------
    std::cout << "\n--- Test 7: Recovery Test (Restarting Engine) ---\n";
    {
        TusuEngine db_restarted("tusu.db");
        assert(db_restarted.get("apple") == "green");
        assert(db_restarted.get("banana") == "sweet-yellow");
        assert(db_restarted.get("cherry") == "red");
        assert(db_restarted.get("strawberry") == "red");
        assert(db_restarted.get("nonexistent") == "NOT FOUND");
        std::cout << "[PASS] WAL Recovery works on restart!\n";
    }

    std::cout << "\n========================================\n";
    std::cout << "🎉 ALL TESTS PASSED SUCCESSFULLY! 🎉\n";
    std::cout << "========================================\n";
}

int main() 
{
    runTests();
    return 0;
}