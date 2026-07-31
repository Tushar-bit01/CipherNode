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
    
    // Check if previous keys still accessible via SSTable fallback
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
    // Ensure deleting one key didn't break others
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
    // Test 6: Persistence / Recovery from WAL on Restart
    // -------------------------------------------------------------
    std::cout << "\n--- Test 6: Recovery Test (Restarting Engine) ---\n";
    {
        // Scope block forces destructor / closing
        TusuEngine db_restarted("tusu.db");
        assert(db_restarted.get("apple") == "green");
        assert(db_restarted.get("banana") == "sweet-yellow");
        assert(db_restarted.get("cherry") == "red");
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