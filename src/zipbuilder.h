#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class zip_builder {
public:
    bool add(const std::string &name, const unsigned char *plain_bytes, size_t length);
    void finish();

    const std::vector<unsigned char> &bytes() const;

private:
    struct stored_entry {
        std::string name;
        uint32_t checksum;
        uint32_t compressed_size;
        uint32_t uncompressed_size;
        uint16_t compression_method;
        uint32_t local_header_offset;
    };

    std::vector<unsigned char> archive_bytes;
    std::vector<stored_entry> stored_entries;
};

