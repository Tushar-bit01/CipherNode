#include<fstream>
#include<string>
#include "../include/binary_storage.h"
#include "../include/binary_record.h"
#include <fcntl.h>
#include <unistd.h>

// uint64_t writeRecord(const std::string &filename,const std::string &key,const std::string &value){
//         std::ofstream outfile(filename,std::ios::binary | std::ios::app);
//         if(!outfile.is_open()){
//             return 0;
//         }
//         uint64_t offset=outfile.tellp();
//         RecordHeader header{};
//         header.keySize=static_cast<uint32_t>(key.size());
//         header.valueSize=static_cast<uint32_t>(value.size());
//         outfile.write(reinterpret_cast<const char*>(&header),sizeof(RecordHeader));
//         outfile.write(key.data(),header.keySize);
//         outfile.write(value.data(),header.valueSize);
//         outfile.close();
//         return offset;
// }

// uint64_t writeTombstoneRecord(const std::string &filename, const std::string &key)
// {
//     std::ofstream outfile(filename, std::ios::binary | std::ios::app);
//     if (!outfile.is_open())
//         return 0;

//     uint64_t offset = outfile.tellp();

//     RecordHeader header{};
//     header.is_tombstone = 1; // Mark as tombstone
//     header.keySize = static_cast<uint32_t>(key.size());
//     header.valueSize = 0;    // Deletions have no value payload

//     outfile.write(reinterpret_cast<const char *>(&header), sizeof(RecordHeader));
//     outfile.write(key.data(), key.size());
//     outfile.close();
//     return offset;
// }

uint64_t writeRecord(
    int fd,
    const std::string &key,
    const std::string &value)
{
    RecordHeader header{};

    header.keySize =
        static_cast<uint32_t>(key.size());

    header.valueSize =
        static_cast<uint32_t>(value.size());

    off_t offset = lseek(fd, 0, SEEK_END);

    if (offset == -1)
        return 0;

    if (write(fd, &header, sizeof(RecordHeader))
        != sizeof(RecordHeader))
        return 0;

    if (write(fd, key.data(), header.keySize)
        != static_cast<ssize_t>(header.keySize))
        return 0;

    if (write(fd, value.data(), header.valueSize)
        != static_cast<ssize_t>(header.valueSize))
        return 0;

    return static_cast<uint64_t>(offset);
}

uint64_t writeTombstoneRecord(
    int fd,
    const std::string &key)
{
    RecordHeader header{};

    header.is_tombstone = 1;
    header.keySize =
        static_cast<uint32_t>(key.size());
    header.valueSize = 0;

    off_t offset = lseek(fd, 0, SEEK_END);

    if (offset == -1)
        return 0;

    if (write(fd, &header, sizeof(RecordHeader))
        != sizeof(RecordHeader))
        return 0;

    if (write(fd, key.data(), header.keySize)
        != static_cast<ssize_t>(header.keySize))
        return 0;

    return static_cast<uint64_t>(offset);
}