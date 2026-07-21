#include <iostream>
#include "engine.h"

int main() {
    // This triggers your recovery constructor! 
    // It should scan tusu.db and rebuild the memtable in RAM automatically.
    TusuEngine db("tusu.db");

    std::cout << "\nAttempting to read data after application restart...\n";
    std::string val1 = db.get("Tushar");
    std::string val2 = db.get("Divyansh");

    std::cout << "Get(Tushar): " << val1 << "\n";
    std::cout << "Get(Divyansh): " << val2 << "\n";

    return 0;
}