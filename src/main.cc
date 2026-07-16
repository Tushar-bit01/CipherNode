#include<iostream>
#include "../include/binary_record.h"
#include "../include/binary_storage.h"

int main(){
    std::string db_file="tusu.db";
    uint64_t pos1=writeRecord(db_file,"Tushar","C++ Developer");
    std::cout<<"position 1 offset"<<pos1<<std::endl;
    uint64_t pos2=writeRecord(db_file,"Divya","Python Developer");
    std::cout<<"position 2 offset"<<pos2<<std::endl;
    std::cout << "Data successfully written to " << db_file << std::endl;
    return 0;
}