#pragma once

#include "package.h"

#include <cstdio>
#include <string>

class nlpx_cipher {
public:
    nlpx_cipher();

    void reset_for_file_size(unsigned int file_size);
    void seek(unsigned int position);

    unsigned char decrypt_byte(unsigned char cipher_byte);
    unsigned char encrypt_byte(unsigned char plain_byte);

private:
    void make_substitution_tables(unsigned int seed);

    unsigned char decrypt_table[256];
    unsigned char encrypt_table[256];
    unsigned char counter;
};

class nlpx_file : public package {
public:
    nlpx_file();
    ~nlpx_file() override;

    bool open(const std::string &path);

    uint64_t size() const override;
    size_t read(uint64_t offset, void *destination, size_t length) override;

    static bool write(const std::string &path, const unsigned char *zip_bytes, uint64_t zip_length);

private:
    FILE *stream;
    uint64_t file_size;
    nlpx_cipher cipher;
};
