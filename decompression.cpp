#include <iostream>
#include <fstream>
#include </home/vladstefan/Personal/Huffman-Text-Compression-Algorithm/huffman.h>

using namespace std;

void read_binary_file(ifstream &input_file, map<char, int> &freq_dict, string &bit_stream)
{
    int freq_size = 0;

    input_file.read((char *)&freq_size, sizeof(freq_size));

    for (int i = 0; i < freq_size; ++i)
    {
        char c;
        int freq;
        input_file.read(&c, sizeof(c));
        input_file.read((char *)&freq, sizeof(freq));
        freq_dict[c] = freq;
    }

    unsigned char current_byte;

    while (input_file.read((char *)&current_byte, sizeof(current_byte)))
    {

        unsigned char mask = 128;

        while (mask >= 1)
        {

            if ((current_byte & mask) != 0)
            {
                bit_stream += '1';
            }
            else
            {
                bit_stream += '0';
            }
            mask = mask >> 1;
        }
    }
}

void debugg_read_all_from_binary_file(ifstream &input_file)
{

    char line;
    ofstream out_file("binaryTest.huff", ios::binary);
    while (input_file.read(&line, sizeof(line)))
    {
        out_file.write(&line, sizeof(line));
    }
    return;
}

string reassamble_text(Node *root, const string &bit_stream, long unsigned int total_chars)
{
    string decoded_text = "";
    if (root == nullptr || bit_stream.empty())
        return decoded_text;

    Node *current = root;

    for (char bit : bit_stream)
    {
        if (decoded_text.size() == total_chars)
        {
            break;
        }

        if (bit == '1')
        {
            current = current->left_child;
        }
        else
        {
            current = current->right_child;
        }

        if (current == nullptr)
        {
            break;
        }

        if (current->is_leaf)
        {
            decoded_text += current->symbol;
            current = root;
        }
    }

    return decoded_text;
}

int main()
{
    Huffman_Tree *huffman_tree = new Huffman_Tree();
    map<char, int> freq_dict;
    vector<pair<char, int>> huffman_vector;
    string bit_stream;
    string text;
    ifstream binary_file("compressedBinary.huff", ios::binary);
    read_binary_file(binary_file, freq_dict, bit_stream);

    huffman_vector = build_huffman_vector(freq_dict);
    huffman_tree->build_huffman_tree(huffman_vector, 0);

    long unsigned int total_chars = 0;
    for (const auto &[ch, freq] : freq_dict)
    {
        total_chars += freq;
    }


    text = reassamble_text(huffman_tree->root, bit_stream, total_chars);
    

    return 0;
}