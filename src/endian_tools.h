#pragma once

#include <cstdint>
#include <vector>

uint16_t little_endian_16(const unsigned char *bytes);
uint32_t little_endian_32(const unsigned char *bytes);
uint64_t big_endian_64(const unsigned char *bytes);

void append_little_endian_16(std::vector<unsigned char> &target, uint16_t value);
void append_little_endian_32(std::vector<unsigned char> &target, uint32_t value);
