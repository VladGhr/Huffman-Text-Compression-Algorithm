#ifndef DECOMPRESSION_H
#define DECOMPRESSION_H

#include "utils.h"

using namespace std;

/**
 * @brief Decompresses a .huff file back into the original bytes, restoring the original extension.
 *        The output goes next to the .huff file; an existing file is never overwritten
 *        (a "_decompressed" suffix is added instead).
 * @param compressed_file Path to the .huff file
 * @return Path of the decompressed file
 * @throws runtime_error if the file can't be read, is not a valid .huff file, or is corrupted
 */
inline string decompress(const string &compressed_file)
{
    ifstream binary_file(filesystem::u8path(compressed_file), ios::binary);
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

    pair<string, string> compressed_file_and_extension = separate_extension(compressed_file);
    string decompressed_file = unique_output_path(compressed_file_and_extension.first, original_extension);

    write_string_to_file(decompressed_file, data);

    return decompressed_file;
}

#endif
