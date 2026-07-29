#include "nlpxfile.h"

#include <cstring>
#include <vector>

static const size_t write_block_length = 1 << 20;

nlpx_cipher::nlpx_cipher() {
    memset(decrypt_table, 0, sizeof(decrypt_table));
    memset(encrypt_table, 0, sizeof(encrypt_table));
    counter = 127;
}

void nlpx_cipher::make_substitution_tables(unsigned int seed) {
    unsigned int scaled = seed >> 10;
    while (scaled > 100) scaled /= 100;

    unsigned int stride = (scaled & 1) ? scaled + 10 : scaled + 9;

    unsigned int value = seed & 0xff;
    for (unsigned int index = 0; index < 256; index++) {
        encrypt_table[index] = (unsigned char)value;
        decrypt_table[value] = (unsigned char)index;

        value += stride;
        if (value >= 256) value -= 256;
    }

    counter = 127;
}

void nlpx_cipher::reset_for_file_size(unsigned int file_size) {
    make_substitution_tables(file_size - 1);
}

void nlpx_cipher::seek(unsigned int position) {
    unsigned int shifted = position + 126;
    counter = (unsigned char)((shifted + shifted / 255 + 1) & 0xff);
}

unsigned char nlpx_cipher::decrypt_byte(unsigned char cipher_byte) {
    unsigned char plain_byte = (unsigned char)~(counter ^ decrypt_table[cipher_byte]);
    counter = (counter == 255) ? 1 : (unsigned char)(counter + 1);
    return plain_byte;
}

unsigned char nlpx_cipher::encrypt_byte(unsigned char plain_byte) {
    unsigned char cipher_byte = encrypt_table[(unsigned char)(~plain_byte ^ counter)];
    counter = (counter == 255) ? 1 : (unsigned char)(counter + 1);
    return cipher_byte;
}

nlpx_file::nlpx_file() {
    stream = nullptr;
    file_size = 0;
}

nlpx_file::~nlpx_file() {
    if (stream) fclose(stream);
}

bool nlpx_file::open(const std::string &path) {
    if (stream) fclose(stream);

    stream = fopen(path.c_str(), "rb");
    if (!stream) return false;

    if (fseek(stream, 0, SEEK_END) != 0) return false;
    long measured_size = ftell(stream);
    if (measured_size <= 0) return false;

    file_size = (uint64_t)measured_size;
    cipher.reset_for_file_size((unsigned int)file_size);

    return true;
}

uint64_t nlpx_file::size() const {
    return file_size;
}

size_t nlpx_file::read(uint64_t offset, void *destination, size_t length) {
    if (!stream || offset >= file_size) return 0;

    if (offset + length > file_size) length = (size_t)(file_size - offset);
    if (fseek(stream, (long)offset, SEEK_SET) != 0) return 0;

    size_t taken = fread(destination, 1, length, stream);
    if (!taken) return 0;

    cipher.seek((unsigned int)offset);

    unsigned char *bytes = (unsigned char *)destination;
    for (size_t position = 0; position < taken; position++)
        bytes[position] = cipher.decrypt_byte(bytes[position]);

    return taken;
}

bool nlpx_file::write(const std::string &path, const unsigned char *zip_bytes, uint64_t zip_length) {
    FILE *target = fopen(path.c_str(), "wb");
    if (!target) return false;

    nlpx_cipher writer;
    writer.reset_for_file_size((unsigned int)zip_length);
    writer.seek(0);

    std::vector<unsigned char> block(write_block_length);

    for (uint64_t written = 0; written < zip_length; written += write_block_length) {
        size_t remaining = (size_t)((zip_length - written < write_block_length) ? zip_length - written : write_block_length);

        for (size_t position = 0; position < remaining; position++)
            block[position] = writer.encrypt_byte(zip_bytes[written + position]);

        if (fwrite(block.data(), 1, remaining, target) != remaining) {
            fclose(target);
            return false;
        }
    }

    fclose(target);
    return true;
}
