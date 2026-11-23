#ifndef COMPRESSION_H
#define COMPRESSION_H

#define COMPRESSED_EXTENSION ".huff"

#include "utils.h"

using namespace std;

/**
 * @brief Compresses any file (text or binary) into "<name>.huff" next to it
 * @param input_file Path to the file to compress
 * @return Path of the compressed file
 * @throws runtime_error if the input can't be read or the output can't be written
 */
inline string compress(const string &input_file)
{
    pair<string, string> file_and_extension = separate_extension(input_file);

    string input_file_name = file_and_extension.first;
    string input_file_extension = file_and_extension.second;
    string compressed_file = input_file_name + COMPRESSED_EXTENSION;

    string data = read_file_to_string(input_file);

    map<unsigned char, uint64_t> freq_dict = create_freq_dict(data);

    priority_queue<Node *, vector<Node *>, Compare> min_heap;
    Huffman_Tree huffman_tree;

    build_huffman_heap(freq_dict, min_heap);
    huffman_tree.build_huffman_tree(min_heap);

    map<unsigned char, string> encoding_dict = create_encoding_dict(huffman_tree);
    vector<unsigned char> byte_stream = create_byte_stream(data, encoding_dict);

    ofstream outBinaryFile(path_from_utf8(compressed_file), ios::binary);
    if (!outBinaryFile)
        throw runtime_error("Can't create: " + compressed_file);

    write_metadata_to_binary_file(outBinaryFile, input_file_extension, freq_dict);
    write_byte_stream_to_binary_file(outBinaryFile, byte_stream);

    if (!outBinaryFile)
        throw runtime_error("Error while writing: " + compressed_file);

    return compressed_file;
}

#endif
