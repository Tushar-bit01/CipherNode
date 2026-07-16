#include<fstream>
#include<string>
#include "../include/binary_storage.h"
#include "../include/binary_record.h"

uint64_t writeRecord(const std::string &filename,const std::string &key,const std::string &value){
        std::ofstream outfile(filename,std::ios::binary | std::ios::app);
        if(!outfile.is_open()){
            return 0;
        }
        uint64_t offset=outfile.tellp();
        RecordHeader header;
        header.keySize=static_cast<uint32_t>(key.size());
        header.valueSize=static_cast<uint32_t>(value.size());
        outfile.write(reinterpret_cast<const char*>(&header),sizeof(RecordHeader));
        outfile.write(key.data(),header.keySize);
        outfile.write(value.data(),header.valueSize);
        outfile.close();
        return offset;
}