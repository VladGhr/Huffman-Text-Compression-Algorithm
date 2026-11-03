#include <iostream>
#include <fstream>
#include <sstream>
#include "classes.h"
#include "huffman.h"


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

string reassamble_text(Node *root, const string &bit_stream, long unsigned int total_chars)
{
    string decoded_text = "";

    if (root == nullptr)
        return decoded_text;

    Node *current = root;

    if (current->is_leaf == true) {
        for(int i = 0; i < current->freq; i++) {
            decoded_text += current->symbol;
        }
        return decoded_text;
    }

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

void write_decoded_text_to_file(ofstream &file, const string &decoded_text)
{
    file << decoded_text;
}


/**
 * @brief Separates the file name and the extension
 * @param file The file with its extension
 * @return Returns a pair:first element from the pair is the name of the file | second element from the pair is the extension          
 */
pair<string, string> separate_extension(string file) {

    vector <string> tokens;
    pair<string, string> file_and_extension;

    stringstream check1(file);

    string intermediate;

    while(getline(check1, intermediate, '.')) {
        tokens.push_back(intermediate);
    }

    file_and_extension.first = tokens[0];
    file_and_extension.second = tokens[1];

    return file_and_extension;  
}

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

string read_file_to_string(const std::string &filename)
{

    ifstream inFile(filename, std::ios::binary);

    if (!inFile)
    {
        cerr << "Cant open: " << filename << endl;
        cerr << filesystem::current_path() << endl;
        return "";
    }

    string s = "";
    char c;

    while (inFile.get(c))
    {
        s += c;
    }
    inFile.close();

    return s;
}