#include <iostream>
#include <cassert>
#include "../include/engine.h"

void runTests()
{
    system("rm -f tusu.db sstable_*.db");

    std::cout << "[INFO] Starting TusuDB Tests\n";

    TusuEngine db("tusu.db");

    //-------------------------------------------------
    // Test 1
    //-------------------------------------------------

    std::cout << "\n[TEST 1] Basic Put/Get\n";

    db.put("apple","red");
    db.put("banana","yellow");

    assert(db.get("apple")=="red");
    assert(db.get("banana")=="yellow");

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 2
    //-------------------------------------------------

    std::cout<<"\n[TEST 2] Update Existing Key\n";

    db.put("apple","green");

    assert(db.get("apple")=="green");

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 3
    //-------------------------------------------------

    std::cout<<"\n[TEST 3] Delete Existing Key\n";

    db.remove("banana");

    assert(db.get("banana")=="NOT FOUND");

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 4
    //-------------------------------------------------

    std::cout<<"\n[TEST 4] Delete Non Existing Key\n";

    db.remove("xyz");

    assert(db.get("xyz")=="NOT FOUND");

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 5
    //-------------------------------------------------

    std::cout<<"\n[TEST 5] Resurrection\n";

    db.put("banana","sweet-yellow");

    assert(db.get("banana")=="sweet-yellow");

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 6
    //-------------------------------------------------

    std::cout<<"\n[TEST 6] Flush Trigger\n";

    db.put("c","1");
    db.put("d","2");
    db.put("e","3");
    db.put("f","4");

    assert(db.get("apple")=="green");
    assert(db.get("banana")=="sweet-yellow");
    assert(db.get("c")=="1");
    assert(db.get("d")=="2");
    assert(db.get("e")=="3");
    assert(db.get("f")=="4");

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 7
    //-------------------------------------------------

    std::cout<<"\n[TEST 7] Large Number of Flushes\n";

    for(int i=0;i<30;i++)
    {
        db.put("key"+std::to_string(i),
               "value"+std::to_string(i));
    }

    for(int i=0;i<30;i++)
    {
        assert(
            db.get("key"+std::to_string(i))
            ==
            "value"+std::to_string(i)
        );
    }

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 8
    //-------------------------------------------------

    std::cout<<"\n[TEST 8] Compaction\n";

    db.put("apple","dark-green");

    for(int i=30;i<60;i++)
    {
        db.put("key"+std::to_string(i),
               "value"+std::to_string(i));
    }

    assert(db.get("apple")=="dark-green");

    for(int i=0;i<60;i++)
    {
        assert(
            db.get("key"+std::to_string(i))
            ==
            "value"+std::to_string(i)
        );
    }

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 9
    //-------------------------------------------------

    std::cout<<"\n[TEST 9] Tombstone Garbage Collection\n";

    db.put("ghost","alive");

    db.remove("ghost");

    for(int i=60;i<90;i++)
    {
        db.put("key"+std::to_string(i),
               "value"+std::to_string(i));
    }

    assert(db.get("ghost")=="NOT FOUND");

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 10
    //-------------------------------------------------

    std::cout<<"\n[TEST 10] Restart Recovery\n";

    {
        TusuEngine restart("tusu.db");

        assert(restart.get("apple")=="dark-green");

        for(int i=0;i<90;i++)
        {
            assert(
                restart.get("key"+std::to_string(i))
                ==
                "value"+std::to_string(i)
            );
        }

        assert(restart.get("ghost")=="NOT FOUND");
    }

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 11
    //-------------------------------------------------

    std::cout<<"\n[TEST 11] Multiple Updates Across SSTables\n";

    db.put("counter","1");

    for(int i=0;i<8;i++)
        db.put("dummy"+std::to_string(i),"x");

    db.put("counter","2");

    for(int i=8;i<16;i++)
        db.put("dummy"+std::to_string(i),"x");

    db.put("counter","3");

    assert(db.get("counter")=="3");

    std::cout<<"PASS\n";

    //-------------------------------------------------
    // Test 12
    //-------------------------------------------------

    std::cout<<"\n[TEST 12] Missing Key\n";

    assert(db.get("i_do_not_exist")=="NOT FOUND");

    std::cout<<"PASS\n";

    std::cout<<"\n=========================================\n";
    std::cout<<"ALL TESTS PASSED\n";
    std::cout<<"=========================================\n";
}

int main()
{
    runTests();
}