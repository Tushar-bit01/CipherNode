#include<iostream>
#include "../include/binary_record.h"
#include "../include/binary_storage.h"

int main(){
    std::string db_file="tusu.db";
    writeRecord(db_file,"Tushar","C++ Developer");
    writeRecord(db_file,"Divya","Python Developer");
    std::cout << "Data successfully written to " << db_file << std::endl;
    return 0;
}