# nlpz

Magic NLP4, then a zip archive under two layers of AES-128 in counter mode.

    [4 NLP4] [payload] [trailer] [8 trailer length]

    payload length + trailer length + 12 == file size
    smallest package is 484 bytes

Building the outer key:

- 28 constant bytes taken from the game binary
- then the file size as big endian 32 bit, which gives every package its own keystream
- of those 32 bytes the first 16 are the AES key, the last 16 are the counter block

Reading order:

- the trailer length field and the trailer run through the outer keystream back to front,
  keystream byte 0 lands on the last byte of the region
- the length field takes keystream 0 to 8, the trailer continues at 8
- the payload runs forward from keystream 0, so keystream position equals payload offset
- any payload offset therefore decrypts on its own

The trailer, offset and length:

    0    8   payload length, big endian
    8   16   AES key for the payload
    24  16   counter block for the payload
    40   -   RSA 3072 signature, then unused bytes

The public key that checks the signature sits in the game binary behind a XOR with one
constant byte. Reading needs no signature, the payload length checks against the file size.
Writing a package the game accepts needs the private key.
