# nolimits-nlp

Extracts and packs the data package containers NoLimits ships its game content in, the
`.nlpx` and `.nlpz` files under its `data` directory. Both hold a zip archive behind a
cipher.

## Download

Binaries for Linux, macOS and Windows are on the
[releases page](https://github.com/geforcefan/nolimits-nlp/releases), each next to its
`.sha256`.

> [!WARNING]
> The release binaries are unsigned, so both systems flag them.
>
> macOS Gatekeeper blocks a downloaded binary. Clear the quarantine flag once:
>
> ```sh
> xattr -dr com.apple.quarantine nolimits-nlp
> ```
>
> Windows Defender may report a threat. It is a false positive. The executable imports only
> `KERNEL32.dll`, so it has no network, registry or process access at all, and it is built
> by GitHub Actions from the source in this repository. Building from source avoids both
> warnings.

## Examples

```sh
nolimits-nlp extract data2603.nlpz trees

nolimits-nlp extract data2603.nlpz data2606.nlpz packages

nolimits-nlp extract --merge data2000.nlpx data2002.nlpx data2003.nlpx \
                             data2603.nlpz data2604.nlpz data2606.nlpz merged

nolimits-nlp pack trees trees.nlpx
```

Without `--merge` several packages go into one subdirectory each. With `--merge` they
overlay into one tree, ordered by base name, and the highest named one wins.

Every extracted entry is checked against its recorded CRC32. Exit code is 2 when an entry
fails.

> [!WARNING]
> Packing to `.nlpz` is not possible and the tool refuses it. An nlpz trailer carries an RSA
> 3072 signature over the payload key, and issuing one needs the private key that only the
> vendor has, so the game rejects any nlpz that was not built by them. Pack `.nlpx` instead,
> that format carries no signature and the game loads it.

Formats: [nlpx](docs/nlpx.md), [nlpz](docs/nlpz.md).

## Build

Needs zlib and a C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```
