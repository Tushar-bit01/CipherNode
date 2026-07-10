#include<fstream>
#include<string>
#include "../include/binary_storage.h"
#include "../include/binary_record.h"

void writeRecord(const std::string &filename,const std::string &key,const std::string &value){
        std::ofstream outfile(filename,std::ios::binary | std::ios::app);
        if(!outfile.is_open()){
            return;
        }
        RecordHeader header;
        header.keySize=static_cast<uint32_t>(key.size());
        header.valueSize=static_cast<uint32_t>(value.size());
        outfile.write(reinterpret_cast<const char*>(&header),sizeof(RecordHeader));
        outfile.write(key.data(),header.keySize);
        outfile.write(value.data(),header.valueSize);
        outfile.close();
}