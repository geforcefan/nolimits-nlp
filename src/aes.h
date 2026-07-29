#pragma once

class aes128 {
public:
    explicit aes128(const unsigned char key[16]);

    void encrypt_block(const unsigned char input[16], unsigned char output[16]) const;

private:
    unsigned char round_keys[11][16];
};
