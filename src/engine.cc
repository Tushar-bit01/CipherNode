#include "../include/engine.h";

TusuEngine::TusuEngine(const std::string &filename):db_file(filename){
    
}

void TusuEngine::put(const std::string &key, const std::string &value){
    int offset=writeRecord("db_file",key,value);
    memtable[key]=offset;
}

std::string TusuEngine::get(const std::string &key){
    if(memtable.find(key)==memtable.end()){
        return "NOT FOUND";
    }
    return "Value_from_Disk_placeholder";
}