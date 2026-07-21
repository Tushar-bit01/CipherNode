#include <iostream>
#include "../include/engine.h"

int main() {
    // 1. Initialize the engine with a database file
    TusuEngine db("tusu.db");

    // 2. Put some data
    std::cout << "Writing records to TusuDB...\n";
    db.put("Tushar", "C++ Developer");
    db.put("Divyansh", "Python Developer");

    // 3. Get the data back (this will hit RAM first, then jump to Disk)
    std::string val1 = db.get("Tushar");
    std::string val2 = db.get("Divyansh");
    std::string val3 = db.get("UnknownKey");

    // 4. Output the results
    std::cout << "Get(Tushar): " << val1 << "\n";
    std::cout << "Get(Divyansh): " << val2 << "\n";
    std::cout << "Get(UnknownKey): " << val3 << "\n";

    return 0;
}