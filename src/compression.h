#ifndef COMPRESSION_H
#define COMOPRESSION_H

#define COMPRESSED_EXTENSION ".huff"

#include "utils.h"

using namespace std;


string compress(string input_file) {

    string s;
    string bit_stream;
    string input_file_name;
    string input_file_extension;
    string compressed_file;
    
    pair<string, string> file_and_extension = separate_extension(input_file); 
    
    input_file_name = file_and_extension.first;
    input_file_extension = file_and_extension.second;
    
    compressed_file = input_file_name + COMPRESSED_EXTENSION;
    
    ifstream inputFile(input_file);
    ofstream outBinaryFile(compressed_file, ios::binary);

    map<char, int> freq_dict;
    map<char, string> encoding_dict;
    priority_queue<Node *, vector<Node *>, Compare> min_heap;
    vector<unsigned char> byte_stream;

    Huffman_Tree *huffman_tree = new Huffman_Tree();

    s = read_file_to_string(input_file);
    freq_dict = create_freq_dict(s);

    build_huffman_heap(freq_dict, min_heap);
    huffman_tree->build_huffman_tree(min_heap);
    encoding_dict = create_encoding_dict(*huffman_tree);
    bit_stream = create_bit_stream(s, encoding_dict);
    byte_stream = create_byte_stream(bit_stream);

    write_metadata_to_binary_file(outBinaryFile, freq_dict);
    write_byte_stream_to_binary_file(outBinaryFile, byte_stream);


    return compressed_file;
}

#endif