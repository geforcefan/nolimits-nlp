#include "zipbuilder.h"

#include "endian_tools.h"

#include <zlib.h>

static const size_t deflate_block_length = 1 << 16;

static const uint32_t local_header_signature = 0x04034b50;
static const uint32_t central_header_signature = 0x02014b50;
static const uint32_t end_of_directory_signature = 0x06054b50;

static bool compress_raw(const unsigned char *plain_bytes, size_t length, std::vector<unsigned char> &compressed) {
    z_stream deflater = {};
    if (deflateInit2(&deflater, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;

    deflater.next_in = (Bytef *)plain_bytes;
    deflater.avail_in = (uInt)length;

    std::vector<unsigned char> block(deflate_block_length);
    int status = Z_OK;

    while (status != Z_STREAM_END) {
        deflater.next_out = block.data();
        deflater.avail_out = (uInt)deflate_block_length;

        status = deflate(&deflater, Z_FINISH);
        if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) {
            deflateEnd(&deflater);
            return false;
        }

        compressed.insert(compressed.end(), block.data(), block.data() + (deflate_block_length - deflater.avail_out));
    }

    deflateEnd(&deflater);
    return true;
}

bool zip_builder::add(const std::string &name, const unsigned char *plain_bytes, size_t length) {
    std::vector<unsigned char> compressed;
    if (!compress_raw(plain_bytes, length, compressed)) return false;

    bool keep_plain = compressed.size() >= length;

    stored_entry archive_entry = {
        .name = name,
        .checksum = (uint32_t)crc32(0, plain_bytes, (uInt)length),
        .compressed_size = (uint32_t)(keep_plain ? length : compressed.size()),
        .uncompressed_size = (uint32_t)length,
        .compression_method = (uint16_t)(keep_plain ? 0 : 8),
        .local_header_offset = (uint32_t)archive_bytes.size(),
    };

    append_little_endian_32(archive_bytes, local_header_signature);
    append_little_endian_16(archive_bytes, 20);
    append_little_endian_16(archive_bytes, 0);
    append_little_endian_16(archive_bytes, archive_entry.compression_method);
    append_little_endian_16(archive_bytes, 0);
    append_little_endian_16(archive_bytes, 0);
    append_little_endian_32(archive_bytes, archive_entry.checksum);
    append_little_endian_32(archive_bytes, archive_entry.compressed_size);
    append_little_endian_32(archive_bytes, archive_entry.uncompressed_size);
    append_little_endian_16(archive_bytes, (uint16_t)name.size());
    append_little_endian_16(archive_bytes, 0);

    archive_bytes.insert(archive_bytes.end(), name.begin(), name.end());

    if (keep_plain) archive_bytes.insert(archive_bytes.end(), plain_bytes, plain_bytes + length);
    else archive_bytes.insert(archive_bytes.end(), compressed.begin(), compressed.end());

    stored_entries.push_back(archive_entry);
    return true;
}

void zip_builder::finish() {
    uint32_t directory_offset = (uint32_t)archive_bytes.size();

    for (const stored_entry &archive_entry : stored_entries) {
        append_little_endian_32(archive_bytes, central_header_signature);
        append_little_endian_16(archive_bytes, 20);
        append_little_endian_16(archive_bytes, 20);
        append_little_endian_16(archive_bytes, 0);
        append_little_endian_16(archive_bytes, archive_entry.compression_method);
        append_little_endian_16(archive_bytes, 0);
        append_little_endian_16(archive_bytes, 0);
        append_little_endian_32(archive_bytes, archive_entry.checksum);
        append_little_endian_32(archive_bytes, archive_entry.compressed_size);
        append_little_endian_32(archive_bytes, archive_entry.uncompressed_size);
        append_little_endian_16(archive_bytes, (uint16_t)archive_entry.name.size());
        append_little_endian_16(archive_bytes, 0);
        append_little_endian_16(archive_bytes, 0);
        append_little_endian_16(archive_bytes, 0);
        append_little_endian_16(archive_bytes, 0);
        append_little_endian_32(archive_bytes, 0);
        append_little_endian_32(archive_bytes, archive_entry.local_header_offset);

        archive_bytes.insert(archive_bytes.end(), archive_entry.name.begin(), archive_entry.name.end());
    }

    uint32_t directory_size = (uint32_t)archive_bytes.size() - directory_offset;

    append_little_endian_32(archive_bytes, end_of_directory_signature);
    append_little_endian_16(archive_bytes, 0);
    append_little_endian_16(archive_bytes, 0);
    append_little_endian_16(archive_bytes, (uint16_t)stored_entries.size());
    append_little_endian_16(archive_bytes, (uint16_t)stored_entries.size());
    append_little_endian_32(archive_bytes, directory_size);
    append_little_endian_32(archive_bytes, directory_offset);
    append_little_endian_16(archive_bytes, 0);
}

const std::vector<unsigned char> &zip_builder::bytes() const {
    return archive_bytes;
}

