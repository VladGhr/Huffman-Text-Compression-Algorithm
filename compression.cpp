#include <iostream>
#include <fstream>
#include "huffman.h"

using namespace std;

void write_metadata_to_binary_file(ofstream &file, map<char, int> &freq_dict)
{
    int map_size = freq_dict.size();

    file.write(reinterpret_cast<const char *>(&map_size), sizeof(map_size));

    for (pair<char, int> p : freq_dict)
    {
        file.write(reinterpret_cast<const char *>(&p.first), sizeof(p.first));
        file.write(reinterpret_cast<const char *>(&p.second), sizeof(p.second));
    }

}

void write_byte_stream_to_binary_file(ofstream &file, vector<unsigned char> &byte_stream)
{
    for (unsigned char byte : byte_stream)
    {
        file.write(reinterpret_cast<const char *>(&byte), sizeof(byte));
    }
}

string read_file_to_string(const std::string& filename) {
    
    ifstream inFile(filename, std::ios::binary);

    if (!inFile) {
        return "";
    }

    string s = "";
    char c;

    while (inFile.get(c)) {
        s += c;
    }
    inFile.close();

    return s;
}

int main()
{
    ofstream outBinaryFile("compressedBinary.huff", ios::binary);

    Huffman_Tree *huffman_tree = new Huffman_Tree();

    map<char, int> freq_dict;
    map<char, string> encoding_dict;
    vector<pair<char, int>> v;
    vector<unsigned char> byte_stream;

    string s;

    s = read_file_to_string("input.txt");



    freq_dict = create_freq_dict(s);
    v = build_huffman_vector(freq_dict);
    huffman_tree->build_huffman_tree(v, 0);
    encoding_dict = create_encoding_dict(*huffman_tree);
    string bit_stream = create_bit_stream(s, encoding_dict);
    byte_stream = create_byte_stream(bit_stream);
    

    write_metadata_to_binary_file(outBinaryFile, freq_dict);
    write_byte_stream_to_binary_file(outBinaryFile, byte_stream);
    outBinaryFile.close();

    return 0;
}
