#pragma once

#include "aes.h"
#include "package.h"

#include <cstdio>
#include <memory>
#include <string>

class nlpz_cipher {
public:
    nlpz_cipher(const unsigned char key[16], const unsigned char counter_block[16]);

    void seek(uint64_t byte_offset);
    void apply_forwards(unsigned char *bytes, size_t length);
    void apply_backwards(unsigned char *bytes, size_t length);

private:
    void next_keystream_block();
    unsigned char next_keystream_byte();

    aes128 block_cipher;
    unsigned char first_counter_block[16];
    unsigned char current_counter_block[16];
    unsigned char keystream[16];
    size_t keystream_position;
};

class nlpz_file : public package {
public:
    nlpz_file();
    ~nlpz_file() override;

    static bool has_magic(const std::string &path);

    bool open(const std::string &path);

    uint64_t size() const override;
    size_t read(uint64_t offset, void *destination, size_t length) override;

private:
    bool read_trailer();

    FILE *stream;
    uint64_t file_size;
    uint64_t payload_length;
    std::unique_ptr<nlpz_cipher> payload_cipher;
};
