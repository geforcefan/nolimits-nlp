#include "endian_tools.h"

uint16_t little_endian_16(const unsigned char *bytes) {
    return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

uint32_t little_endian_32(const unsigned char *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

uint64_t big_endian_64(const unsigned char *bytes) {
    uint64_t value = 0;
    for (int byte_index = 0; byte_index < 8; byte_index++)
        value = (value << 8) | bytes[byte_index];
    return value;
}

void append_little_endian_16(std::vector<unsigned char> &target, uint16_t value) {
    target.push_back((unsigned char)(value & 0xff));
    target.push_back((unsigned char)((value >> 8) & 0xff));
}

void append_little_endian_32(std::vector<unsigned char> &target, uint32_t value) {
    target.push_back((unsigned char)(value & 0xff));
    target.push_back((unsigned char)((value >> 8) & 0xff));
    target.push_back((unsigned char)((value >> 16) & 0xff));
    target.push_back((unsigned char)((value >> 24) & 0xff));
}

