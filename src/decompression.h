#ifndef DECOMPRESSION_H
#define DECOMRPESSION_H

#include <utils.h>

using namespace std;

void decompress(string compressed_file)
{
    string text;
    string bit_stream;
    pair<string, string> compressed_file_and_extension = separate_extension(compressed_file);



    string decompressed_file_without_extension = compressed_file_and_extension.first + ".txt";

    Huffman_Tree *huffman_tree = new Huffman_Tree();
    map<char, int> freq_dict;
    priority_queue<Node *, vector<Node *>, Compare> min_heap;

    ifstream binary_file(compressed_file, ios::binary);
    ofstream plain_file("decompressedPlain.txt");
    read_binary_file(binary_file, freq_dict, bit_stream);

    build_huffman_heap(freq_dict, min_heap);
    huffman_tree->build_huffman_tree(min_heap);

    long unsigned int total_chars = 0;
    for (const auto &[ch, freq] : freq_dict)
    {
        total_chars += freq;
    }

    text = reassamble_text(huffman_tree->root, bit_stream, total_chars);
    write_decoded_text_to_file(plain_file, text);
    plain_file.close();
    binary_file.close();
}

#endif
