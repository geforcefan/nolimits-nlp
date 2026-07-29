#pragma once

#include "package.h"

#include <cstdint>
#include <string>
#include <vector>

struct zip_entry {
    std::string name;
    uint16_t compression_method;
    uint32_t checksum;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint64_t data_offset;
};

class zip_archive {
public:
    zip_archive();

    bool open(package *source_package);

    const std::vector<zip_entry> &entries() const;
    const zip_entry *find(const std::string &name) const;
    bool unpack(const zip_entry &archive_entry, std::vector<unsigned char> &plain_bytes);

private:
    bool read_central_directory();
    bool read_payload_offset(uint64_t local_header_offset, uint64_t &payload_offset);

    package *source;
    std::vector<zip_entry> archive_entries;
};
