#include <iostream>
#include <string>
#include "../include/engine.h"

int main() {
    std::cout << "=== Starting TusuEngine Flush & SSTable Test ===" << std::endl;

    TusuEngine db("tusu.db");

    std::cout << "\n[Test 1] Writing records past threshold (limit is 3)..." << std::endl;
    db.put("apple", "red fruit");
    db.put("banana", "yellow fruit");
    db.put("cat", "feline");
    db.put("dog", "canine"); // This 4th insert should automatically trigger flush()!
    db.put("elephant", "mammal");

    std::cout << "\n[Test 2] Reading keys (should pull from SSTable or MemTable seamlessly)..." << std::endl;
    std::cout << "Get 'apple' -> " << db.get("apple") << " (Expected: red fruit)" << std::endl;
    std::cout << "Get 'cat' -> " << db.get("cat") << " (Expected: feline)" << std::endl;
    std::cout << "Get 'elephant' -> " << db.get("elephant") << " (Expected: mammal)" << std::endl;

    std::cout << "\n[Test 3] Testing non-existent key..." << std::endl;
    std::cout << "Get 'zebra' -> " << db.get("zebra") << " (Expected: NOT FOUND)" << std::endl;

    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}