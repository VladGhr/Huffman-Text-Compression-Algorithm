#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include </home/vladstefan/Personal/Huffman-Text-Compression-Algorithm/classes.h>

using namespace std;

map<char, int> create_freq_dict(string s)
{
    map<char, int> freq_dict;

    for (char letter : s)
    {
        freq_dict[letter]++;
    }

    return freq_dict;
}

vector<pair<char, int>> build_huffman_vector(map<char, int> &freq_dict)
{

    vector<pair<char, int>> sorted_freq_dict_elements(freq_dict.begin(), freq_dict.end());

    sort(sorted_freq_dict_elements.begin(), sorted_freq_dict_elements.end(), [](pair<char, int> &a, pair<char, int> &b)
         { return a.second < b.second; });

    return sorted_freq_dict_elements;
}

void dfs(map<char, string> &encoding_dict, Node *root, string s)
{

    if (root == nullptr)
    {
        return;
    }

    if (root->is_leaf == true)
    {
        encoding_dict[root->symbol] = s;
        return;
    }

    // go left
    dfs(encoding_dict, root->left_child, s + '1');
    // go right
    dfs(encoding_dict, root->right_child, s + '0');
}

map<char, string> create_encoding_dict(Huffman_Tree &huffman_tree)
{
    // left edge = 1, right edge = 0

    map<char, string> encoding_dict;
    dfs(encoding_dict, huffman_tree.root, "");

    return encoding_dict;
}

string create_bit_stream(string &text, map<char, string> &encoding_dict)
{
    string bit_stream;

    for (char c : text)
    {
        bit_stream += encoding_dict[c];
    }

    return bit_stream;
}

vector<unsigned char> create_byte_stream(string bit_stream)
{
    vector<unsigned char> byte_stream;
    unsigned char current_byte = 0;
    int bit_count = 0;

    for (char bit_char : bit_stream)
    {
        current_byte = current_byte << 1;

        if (bit_char == '1')
            current_byte = current_byte | 1;

        bit_count++;

        if (bit_count == 8)
        {
            byte_stream.push_back(current_byte);
            current_byte = 0;
            bit_count = 0;
        }
    }

    if(bit_count > 0) {
        current_byte = current_byte << (8 - bit_count);
        byte_stream.push_back(current_byte);
    }

    return byte_stream;
}

