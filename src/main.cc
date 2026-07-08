#include<iostream>
#include "../include/binary_record.h"

void writeRecord(const std::string &filename,const std::string &key,const std::string &value);
int main(){
    std::string db_file="tusu.db";
    writeRecord(db_file,"Tushar","C++ Developer");
    writeRecord(db_file,"Divya","Python Developer");
    std::cout << "Data successfully written to " << db_file << std::endl;
    return 0;
}