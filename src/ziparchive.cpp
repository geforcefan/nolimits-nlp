#include "ziparchive.h"

#include "endian_tools.h"

#include <zlib.h>

static const uint32_t local_header_signature = 0x04034b50;
static const uint32_t central_header_signature = 0x02014b50;
static const uint32_t end_of_directory_signature = 0x06054b50;

static const size_t local_header_length = 30;
static const size_t central_header_length = 46;
static const size_t end_of_directory_length = 22;
static const size_t directory_search_length = 66000;
static const size_t inflate_block_length = 1 << 16;

zip_archive::zip_archive() {
    source = nullptr;
}

bool zip_archive::open(package *source_package) {
    source = source_package;
    archive_entries.clear();

    return read_central_directory();
}

bool zip_archive::read_payload_offset(uint64_t local_header_offset, uint64_t &payload_offset) {
    unsigned char header[local_header_length];
    if (source->read(local_header_offset, header, local_header_length) != local_header_length) return false;
    if (little_endian_32(header) != local_header_signature) return false;

    payload_offset = local_header_offset + local_header_length
                   + little_endian_16(header + 26) + little_endian_16(header + 28);
    return true;
}

bool zip_archive::read_central_directory() {
    uint64_t total_length = source->size();
    if (total_length < end_of_directory_length) return false;

    size_t tail_length = (size_t)((total_length < directory_search_length) ? total_length : directory_search_length);
    std::vector<unsigned char> tail(tail_length);

    if (source->read(total_length - tail_length, tail.data(), tail_length) != tail_length) return false;

    long directory_record = -1;
    for (long candidate = (long)(tail_length - end_of_directory_length); candidate >= 0; candidate--) {
        if (little_endian_32(tail.data() + candidate) == end_of_directory_signature) {
            directory_record = candidate;
            break;
        }
    }
    if (directory_record < 0) return false;

    uint16_t announced_count = little_endian_16(tail.data() + directory_record + 10);
    uint32_t directory_offset = little_endian_32(tail.data() + directory_record + 16);
    if (directory_offset >= total_length) return false;

    uint64_t cursor = directory_offset;
    for (uint16_t read_count = 0; read_count < announced_count; read_count++) {
        unsigned char header[central_header_length];
        if (source->read(cursor, header, central_header_length) != central_header_length) return false;
        if (little_endian_32(header) != central_header_signature) return false;

        uint16_t name_length = little_endian_16(header + 28);
        uint16_t extra_length = little_endian_16(header + 30);
        uint16_t comment_length = little_endian_16(header + 32);

        std::vector<char> name_bytes(name_length);
        if (name_length && source->read(cursor + central_header_length, name_bytes.data(), name_length) != name_length)
            return false;

        uint64_t payload_offset = 0;
        if (!read_payload_offset(little_endian_32(header + 42), payload_offset)) return false;

        archive_entries.push_back({
            .name = std::string(name_bytes.data(), name_length),
            .compression_method = little_endian_16(header + 10),
            .checksum = little_endian_32(header + 16),
            .compressed_size = little_endian_32(header + 20),
            .uncompressed_size = little_endian_32(header + 24),
            .data_offset = payload_offset,
        });

        cursor += central_header_length + name_length + extra_length + comment_length;
    }

    return !archive_entries.empty();
}

const std::vector<zip_entry> &zip_archive::entries() const {
    return archive_entries;
}

const zip_entry *zip_archive::find(const std::string &name) const {
    const zip_entry *newest = nullptr;
    for (const zip_entry &archive_entry : archive_entries) {
        if (archive_entry.name == name) newest = &archive_entry;
    }
    return newest;
}

bool zip_archive::unpack(const zip_entry &archive_entry, std::vector<unsigned char> &plain_bytes) {
    plain_bytes.clear();
    plain_bytes.reserve(archive_entry.uncompressed_size);

    if (archive_entry.compression_method == 0) {
        plain_bytes.resize(archive_entry.compressed_size);
        if (source->read(archive_entry.data_offset, plain_bytes.data(), plain_bytes.size()) != plain_bytes.size())
            return false;
        return crc32(0, plain_bytes.data(), (uInt)plain_bytes.size()) == archive_entry.checksum;
    }

    z_stream inflater = {};
    if (inflateInit2(&inflater, -15) != Z_OK) return false;

    std::vector<unsigned char> compressed(inflate_block_length);
    std::vector<unsigned char> expanded(inflate_block_length);

    uint64_t consumed = 0;
    int status = Z_OK;

    while (consumed < archive_entry.compressed_size && status != Z_STREAM_END) {
        size_t wanted = (size_t)(archive_entry.compressed_size - consumed);
        if (wanted > inflate_block_length) wanted = inflate_block_length;

        size_t taken = source->read(archive_entry.data_offset + consumed, compressed.data(), wanted);
        if (!taken) break;

        consumed += taken;
        inflater.next_in = compressed.data();
        inflater.avail_in = (uInt)taken;

        do {
            inflater.next_out = expanded.data();
            inflater.avail_out = (uInt)inflate_block_length;

            status = inflate(&inflater, Z_NO_FLUSH);
            if (status == Z_NEED_DICT || status == Z_DATA_ERROR || status == Z_MEM_ERROR) {
                inflateEnd(&inflater);
                return false;
            }

            plain_bytes.insert(plain_bytes.end(), expanded.data(),
                               expanded.data() + (inflate_block_length - inflater.avail_out));
        } while (inflater.avail_out == 0);
    }

    inflateEnd(&inflater);

    if (plain_bytes.size() != archive_entry.uncompressed_size) return false;
    return crc32(0, plain_bytes.data(), (uInt)plain_bytes.size()) == archive_entry.checksum;
}

