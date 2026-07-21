#include "../include/engine.h"
#include<fstream>
#include<iostream>

TusuEngine::TusuEngine(const std::string &filename):db_file(filename){};

void TusuEngine::put(const std::string &key, const std::string &value){
    int offset=writeRecord(db_file,key,value);
    memtable[key]=offset;
}

std::string TusuEngine::get(const std::string &key){
    if(memtable.find(key)==memtable.end()){
        return "NOT FOUND";
    }
    auto it = memtable.find(key);
    uint64_t offset = it->second;
    
    std::ifstream infile(db_file,std::ios::binary);
    if(!infile.is_open()) return "FILE ERROR";
    infile.seekg(offset);

    RecordHeader header;
    infile.read(reinterpret_cast<char*>(&header),sizeof(RecordHeader));
    //skip keysize bytes
    infile.seekg(header.keySize, std::ios::cur);
    //make buffer of size value
    std::string value(header.valueSize, '\0');
    infile.read(&value[0], header.valueSize);

    return value;
}