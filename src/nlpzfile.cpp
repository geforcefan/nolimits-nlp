#include "nlpzfile.h"

#include "endian_tools.h"

#include <cstring>
#include <vector>

static const char magic[4] = {'N', 'L', 'P', '4'};
static const char master_key_text[] = "niaRNsraeTekiLemiTNtsoLBlliW";
static const size_t master_key_text_length = 28;

static const uint64_t magic_length = 4;
static const uint64_t trailer_size_field_length = 8;
static const uint64_t trailer_header_length = 40;
static const uint64_t smallest_package = 484;

static void make_master_key(uint32_t file_size, unsigned char key[16], unsigned char counter_block[16]) {
    unsigned char material[32];
    memcpy(material, master_key_text, master_key_text_length);

    material[28] = (unsigned char)((file_size >> 24) & 0xff);
    material[29] = (unsigned char)((file_size >> 16) & 0xff);
    material[30] = (unsigned char)((file_size >> 8) & 0xff);
    material[31] = (unsigned char)(file_size & 0xff);

    memcpy(key, material, 16);
    memcpy(counter_block, material + 16, 16);
}

nlpz_cipher::nlpz_cipher(const unsigned char key[16], const unsigned char counter_block[16])
    : block_cipher(key) {
    memcpy(first_counter_block, counter_block, 16);
    seek(0);
}

void nlpz_cipher::next_keystream_block() {
    block_cipher.encrypt_block(current_counter_block, keystream);

    for (int byte_index = 15; byte_index >= 0; byte_index--) {
        if (++current_counter_block[byte_index] != 0) break;
    }

    keystream_position = 0;
}

void nlpz_cipher::seek(uint64_t byte_offset) {
    memcpy(current_counter_block, first_counter_block, 16);

    uint64_t block_index = byte_offset / 16;
    unsigned int carry = 0;
    for (int byte_index = 15; byte_index >= 0; byte_index--) {
        unsigned int sum = current_counter_block[byte_index] + (unsigned int)(block_index & 0xff) + carry;
        current_counter_block[byte_index] = (unsigned char)(sum & 0xff);
        carry = sum >> 8;
        block_index >>= 8;
        if (!block_index && !carry) break;
    }

    next_keystream_block();
    keystream_position = (size_t)(byte_offset % 16);
}

unsigned char nlpz_cipher::next_keystream_byte() {
    if (keystream_position == 16) next_keystream_block();
    return keystream[keystream_position++];
}

void nlpz_cipher::apply_forwards(unsigned char *bytes, size_t length) {
    for (size_t position = 0; position < length; position++)
        bytes[position] = (unsigned char)(bytes[position] ^ next_keystream_byte());
}

void nlpz_cipher::apply_backwards(unsigned char *bytes, size_t length) {
    for (size_t position = 0; position < length; position++)
        bytes[length - 1 - position] = (unsigned char)(bytes[length - 1 - position] ^ next_keystream_byte());
}

nlpz_file::nlpz_file() {
    stream = nullptr;
    file_size = 0;
    payload_length = 0;
}

nlpz_file::~nlpz_file() {
    if (stream) fclose(stream);
}

bool nlpz_file::has_magic(const std::string &path) {
    FILE *probe = fopen(path.c_str(), "rb");
    if (!probe) return false;

    char header[4] = {0, 0, 0, 0};
    bool matches = fread(header, 1, 4, probe) == 4 && memcmp(header, magic, 4) == 0;

    fclose(probe);
    return matches;
}

bool nlpz_file::read_trailer() {
    unsigned char master_key[16];
    unsigned char master_counter_block[16];
    make_master_key((uint32_t)file_size, master_key, master_counter_block);

    nlpz_cipher master(master_key, master_counter_block);

    unsigned char size_field[8];
    if (fseek(stream, (long)(file_size - trailer_size_field_length), SEEK_SET) != 0) return false;
    if (fread(size_field, 1, 8, stream) != 8) return false;

    master.seek(0);
    master.apply_backwards(size_field, 8);

    uint64_t trailer_length = big_endian_64(size_field);
    if (trailer_length < trailer_header_length) return false;
    if (trailer_length + magic_length + trailer_size_field_length > file_size) return false;

    std::vector<unsigned char> trailer((size_t)trailer_length);
    uint64_t trailer_offset = file_size - trailer_size_field_length - trailer_length;

    if (fseek(stream, (long)trailer_offset, SEEK_SET) != 0) return false;
    if (fread(trailer.data(), 1, trailer.size(), stream) != trailer.size()) return false;

    master.seek(trailer_size_field_length);
    master.apply_backwards(trailer.data(), trailer.size());

    payload_length = big_endian_64(trailer.data());
    if (payload_length + trailer_length + magic_length + trailer_size_field_length != file_size) return false;

    payload_cipher.reset(new nlpz_cipher(trailer.data() + 8, trailer.data() + 24));
    return true;
}

bool nlpz_file::open(const std::string &path) {
    if (stream) fclose(stream);

    stream = fopen(path.c_str(), "rb");
    if (!stream) return false;

    if (fseek(stream, 0, SEEK_END) != 0) return false;
    long measured_size = ftell(stream);
    if (measured_size < (long)smallest_package) return false;

    file_size = (uint64_t)measured_size;

    char header[4];
    if (fseek(stream, 0, SEEK_SET) != 0) return false;
    if (fread(header, 1, 4, stream) != 4 || memcmp(header, magic, 4) != 0) return false;

    return read_trailer();
}

uint64_t nlpz_file::size() const {
    return payload_length;
}

size_t nlpz_file::read(uint64_t offset, void *destination, size_t length) {
    if (!stream || !payload_cipher || offset >= payload_length) return 0;

    if (offset + length > payload_length) length = (size_t)(payload_length - offset);
    if (fseek(stream, (long)(magic_length + offset), SEEK_SET) != 0) return 0;

    size_t taken = fread(destination, 1, length, stream);
    if (!taken) return 0;

    payload_cipher->seek(offset);
    payload_cipher->apply_forwards((unsigned char *)destination, taken);

    return taken;
}

