#include <iostream>
#include <fstream>
#include </home/vladstefan/Personal/Huffman-Text-Compression-Algorithm/huffman.h>

using namespace std;

void read_metadata_from_binary_file(ifstream &input_file) {
    int freq_size;
    int freq;
    char c = 0;
    map<char, int> freq_dict;

    input_file.read((char *)&freq_size, sizeof(freq_size));
    cout<<freq_size<<endl;


    do {
        input_file.read((char *)&c, sizeof(c));
        cout<<c<<endl;
        if(c != '\n') {
            input_file.read((char *)&freq, sizeof(freq));
            freq_dict[c] = freq;
        }
    }while(c != '\n');

    for(pair<char, int> p : freq_dict) {
        cout<<p.first << " " << p.second;
    }

    return;
}

int main() {

    ifstream binary_file("compressedBinary.huff", ios::binary);
    read_metadata_from_binary_file(binary_file);


    return 0;
}