#ifndef DECOMPRESSION_H
#define DECOMPRESSION_H

#include "utils.h"

using namespace std;

/**
 * @brief Decompresses a .huff file back into the original bytes, restoring the original extension.
 *        The output goes next to the .huff file; an existing file is never overwritten
 *        (a "_decompressed" suffix is added instead).
 * @param compressed_file Path to the .huff file
 * @param output_file     Optional: exact path to write to (overwritten if it exists).
 *                        Default: next to the .huff file, with the original extension.
 * @return Path of the decompressed file
 * @throws runtime_error if the file can't be read, is not a valid .huff file, or is corrupted
 */
inline string decompress(const string &compressed_file, const string &output_file = "")
{
    ifstream binary_file(path_from_utf8(compressed_file), ios::binary);
    if (!binary_file)
        throw runtime_error("Can't open: " + compressed_file);

    string original_extension;
    map<unsigned char, uint64_t> freq_dict;
    vector<unsigned char> byte_stream;

    read_binary_file(binary_file, original_extension, freq_dict, byte_stream);
    binary_file.close();

    priority_queue<Node *, vector<Node *>, Compare> min_heap;
    Huffman_Tree huffman_tree;

    build_huffman_heap(freq_dict, min_heap);
    huffman_tree.build_huffman_tree(min_heap);

    uint64_t total_symbols = 0;
    for (const auto &p : freq_dict)
        total_symbols += p.second;

    string data = decode_byte_stream(huffman_tree.root, byte_stream, total_symbols);
    if (data.size() != total_symbols)
        throw runtime_error("Corrupted .huff file: payload is shorter than expected");

    string decompressed_file = output_file;
    if (decompressed_file.empty())
    {
        pair<string, string> compressed_file_and_extension = separate_extension(compressed_file);
        decompressed_file = unique_output_path(compressed_file_and_extension.first, original_extension);
    }

    write_string_to_file(decompressed_file, data);

    return decompressed_file;
}

/**
 * @brief Returns the original extension stored in a .huff file (e.g. "zip", "arc"),
 *        without decompressing anything.
 * @throws runtime_error if it is not a valid .huff file
 */
inline string read_original_extension(const string &compressed_file)
{
    ifstream binary_file(path_from_utf8(compressed_file), ios::binary);
    if (!binary_file)
        throw runtime_error("Can't open: " + compressed_file);

    char magic[4];
    if (!binary_file.read(magic, 4) || string(magic, 4) != HUFF_MAGIC)
        throw runtime_error("Not a valid .huff file");

    uint64_t ext_size = read_uint(binary_file, 4);
    if (ext_size > 4096)
        throw runtime_error("Corrupted .huff file: invalid extension length");

    string extension(static_cast<size_t>(ext_size), '\0');
    if (ext_size > 0 && !binary_file.read(&extension[0], static_cast<streamsize>(ext_size)))
        throw runtime_error("Corrupted .huff file: unexpected end of header");

    return extension;
}

#endif
