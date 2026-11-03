# Huffman Text Compression

A command-line text compressor and decompressor written in C++17, based on Huffman coding. The compressor reads a text file, builds a prefix-code tree from character frequencies, and writes a compact binary file. The decompressor rebuilds the same tree from the stored frequency table and decodes the bit stream.

## Table of Contents

- [Features](#features)
- [How It Works](#how-it-works)
  - [Compression pipeline](#compression-pipeline)
  - [Decompression pipeline](#decompression-pipeline)
  - [Tree construction](#tree-construction)
- [File Format](#file-format)
- [Project Structure](#project-structure)
- [Building](#building)
- [Usage](#usage)
- [Debug Output](#debug-output)
- [Complexity](#complexity)
- [Known Limitations](#known-limitations)
- [Roadmap](#roadmap)

## Features

- Character-frequency analysis with `std::map`
- Huffman-style prefix code generation (left edge = `1`, right edge = `0`)
- Bit packing into bytes (MSB first) with zero padding on the last byte
- Self-contained `.huff` binary format (frequency table + payload)
- Decoder that rebuilds the tree from the stored frequencies
- Human-readable debug dump of the code table and bit stream
- Level-order tree printer for inspecting the generated tree

## How It Works

### Compression pipeline

```
input.txt
   │
   ▼
 read text ──► frequency map ──► sorted (symbol, freq) vector
                                          │
                                          ▼
                                   build Huffman tree
                                          │
                                          ▼
                       DFS ──► encoding table (symbol → bit string)
                                          │
                                          ▼
                      text ──► bit stream ("0101...") ──► packed bytes
                                          │
                                          ▼
                              compressedBinary.huff
```

1. **Read input:** `input.txt` is read line by line and concatenated into one string.
2. **Count frequencies:** `create_freq_dict` counts each character.
3. **Sort:** `build_huffman_vector` sorts the `(symbol, frequency)` pairs by ascending frequency.
4. **Build tree:** `Huffman_Tree::build_huffman_tree` constructs the tree (see below).
5. **Generate codes:** `create_encoding_dict` runs a DFS over the tree, appending `1` for a left edge and `0` for a right edge.
6. **Encode:** `create_bit_stream` maps every character to its code, and `create_byte_stream` packs the bits into bytes, padding the final byte with zeros.
7. **Write:** the frequency table and the packed bytes are written to `compressedBinary.huff`.

### Decompression pipeline

1. Read the frequency table from the header of `compressedBinary.huff`.
2. Rebuild the exact same tree using the same sorting and construction code.
3. Expand the remaining bytes back into a bit string.
4. Walk the tree bit by bit (`1` = left, `0` = right), emitting a symbol at every leaf and returning to the root.
5. Stop once the number of decoded characters equals the sum of all frequencies, which makes the zero padding in the last byte harmless.

### Tree construction

The tree is built iteratively over the frequency-sorted vector:

- The two least frequent symbols become the leaves of the first internal node.
- Every following symbol becomes a new leaf attached as the **right child** of a new root, with the previous root as the **left child**.

Because encoder and decoder share this deterministic procedure, only the frequencies need to be stored, not the tree itself.

## File Format

`compressedBinary.huff` layout:

| Offset | Size | Content |
|--------|------|---------|
| 0 | 4 bytes (`int`) | Number of distinct symbols `N` |
| 4 | `N` × 5 bytes | For each symbol: 1 byte `char` + 4 bytes `int` frequency |
| 4 + 5N | remaining | Packed bit stream, MSB first, last byte zero-padded |

The original length is not stored explicitly; it is recovered as the sum of all frequencies.

## Project Structure

| File | Role |
|------|------|
| `classes.h` | `Node` and `Huffman_Tree` classes (tree construction and level-order printing) |
| `huffman.h` | Helpers: frequency map, sorted vector, DFS code generation, bit/byte stream creation |
| compressor `main` | Reads `input.txt`, writes `compressedBinary.huff` and `compressedPlain.huff` |
| decompressor `main` | Reads `compressedBinary.huff`, rebuilds the tree and decodes the text |

> Rename the two `main` files to match your repository (for example `compress.cpp` and `decompress.cpp`).

## Building

Requires a C++17 compiler (structured bindings are used).

```bash
g++ -std=c++17 -Wall -Wextra compress.cpp -o compress
g++ -std=c++17 -Wall -Wextra decompress.cpp -o decompress
```

> The source currently includes the headers through an absolute path. For portability, change those to relative includes:
> `#include "huffman.h"`

## Usage

```bash
# 1. Put the text to compress in the working directory
echo "hello huffman" > input.txt

# 2. Compress
./compress          # produces compressedBinary.huff (and compressedPlain.huff)

# 3. Decompress
./decompress        # rebuilds the tree and decodes the content
```

Input and output file names are currently fixed in the source.

## Debug Output

Alongside the binary file, the compressor writes `compressedPlain.huff` as a readable dump:

1. Number of distinct symbols
2. The code table, one `symbol code` pair per line
3. The full bit stream as a string of `0`/`1` characters
4. The packed bytes written as raw characters

It also prints the sorted `(symbol, frequency)` list to standard output. `Huffman_Tree::print_huffman_tree()` prints the tree level by level (internal nodes show their frequency, leaves show their symbol, spaces appear as `-space_symbol-`).

## Complexity

With `n` input characters and `k` distinct symbols:

| Step | Time |
|------|------|
| Frequency counting | O(n log k) |
| Sorting | O(k log k) |
| Tree construction | O(k) |
| Encoding | O(n · average code length) |
| Decoding | O(total bits) |

The header costs 5 bytes per distinct symbol plus 4 bytes for the count.
