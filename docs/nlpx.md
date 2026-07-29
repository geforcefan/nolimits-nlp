# nlpx

A zip archive with every byte substituted. There is no header and no magic, byte 0 of the
file is already encrypted.

The keystream is a 256 byte permutation plus a counter running 127, 128 up to 255, then 1,
2 and on. Zero never occurs.

    plain = ~(counter ^ table[cipher])

Building the table:

- the seed is the file size minus one
- take `seed >> 10` and divide it by 100 until it is 100 or less
- add 9 when that is even, 10 when it is odd, that is the stride
- an even stride would not visit every value
- walk from `seed & 0xff` in steps of stride over all 256 values
- record for each value the step it was reached at

The counter at any offset is `(offset + 126) mod 255 + 1`, so reading at an offset needs
nothing before it. The permutation inverts, so encrypting is the same walk the other way
round.
