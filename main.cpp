#include <iostream>
#include <fstream>
#include </home/vladstefan/Personal/Huffman-Text-Compression-Algorithm/huffman.h>

using namespace std;

void write_metadata_to_binary_file(ofstream &file, map<char, int> &freq_dict) {
    int map_size = freq_dict.size();

    file.write(reinterpret_cast<const char*>(&map_size), sizeof(map_size));

    for(pair<char, int> p : freq_dict) {
        file.write(reinterpret_cast<const char*>(&p.first), sizeof(p.first));
        file.write(reinterpret_cast<const char*>(&p.second), sizeof(p.second));
    }
    
}

int main() {

    Huffman_Tree *huffman_tree = new Huffman_Tree();

    map<char, int> freq_dict;
    map<char, string> encoding_dict;
    vector<pair<char, int>> v;
    vector<unsigned char> byte_stream;
    
    string s = "banana bandana";

    freq_dict = create_freq_dict("banana bandana");
    v = build_huffman_vector(freq_dict);
    huffman_tree->build_huffman_tree(v, 0);
    encoding_dict = create_encoding_dict(*huffman_tree);
    string bit_stream = create_bit_stream(s, encoding_dict);
    byte_stream = create_byte_stream(bit_stream);
    
    ofstream outBinaryFile("compressedBinary.huff", ios::binary);
    ofstream outPlainFile("compressedPlain.huff");

    write_metadata_to_binary_file(outBinaryFile, freq_dict);

    return 0;
}