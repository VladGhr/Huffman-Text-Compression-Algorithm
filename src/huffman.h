#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include "classes.h"

using namespace std;

// counts how many times each byte value (0..255) appears in the input
inline map<unsigned char, uint64_t> create_freq_dict(const string &s)
{
    map<unsigned char, uint64_t> freq_dict;

    for (char letter : s)
    {
        freq_dict[static_cast<unsigned char>(letter)]++;
    }

    return freq_dict;
}

inline void build_huffman_heap(const map<unsigned char, uint64_t> &freq_dict, priority_queue<Node *, vector<Node *>, Compare> &min_heap)
{
    for (const auto &p : freq_dict)
    {
        min_heap.push(new Node(p.first, p.second, true, p.first, nullptr, nullptr));
    }
}

inline void dfs(map<unsigned char, string> &encoding_dict, Node *root, const string &s)
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

inline map<unsigned char, string> create_encoding_dict(Huffman_Tree &huffman_tree)
{
    // left edge = 1, right edge = 0
    // if the root is a leaf (a single distinct byte), its code is "" and no bits are written:
    // the decoder rebuilds the data from the frequency alone

    map<unsigned char, string> encoding_dict;
    dfs(encoding_dict, huffman_tree.root, "");

    return encoding_dict;
}

// encodes the input directly into packed bytes (MSB first),
// without building an intermediate '0'/'1' string (which would use 8x the memory)
inline vector<unsigned char> create_byte_stream(const string &data, const map<unsigned char, string> &encoding_dict)
{
    // lookup table instead of map lookups for every byte
    vector<const string *> codes(256, nullptr);
    for (const auto &p : encoding_dict)
        codes[p.first] = &p.second;

    vector<unsigned char> byte_stream;
    byte_stream.reserve(data.size() / 2 + 1);

    unsigned char current_byte = 0;
    int bit_count = 0;

    for (char c : data)
    {
        const string &code = *codes[static_cast<unsigned char>(c)];

        for (char bit_char : code)
        {
            current_byte = static_cast<unsigned char>(current_byte << 1);

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
    }

    // pad the last byte with zeros
    if (bit_count > 0)
    {
        current_byte = static_cast<unsigned char>(current_byte << (8 - bit_count));
        byte_stream.push_back(current_byte);
    }

    return byte_stream;
}

// decodes exactly total_symbols bytes from the packed bit stream
inline string decode_byte_stream(Node *root, const vector<unsigned char> &byte_stream, uint64_t total_symbols)
{
    string decoded;

    if (root == nullptr || total_symbols == 0)
        return decoded;

    decoded.reserve(static_cast<size_t>(total_symbols));

    // single distinct byte: there are no bits, only the count
    if (root->is_leaf)
    {
        decoded.assign(static_cast<size_t>(total_symbols), static_cast<char>(root->symbol));
        return decoded;
    }

    Node *current = root;

    for (unsigned char byte : byte_stream)
    {
        for (unsigned char mask = 128; mask != 0; mask >>= 1)
        {
            current = (byte & mask) ? current->left_child : current->right_child;

            if (current == nullptr)
                return decoded; // corrupted stream

            if (current->is_leaf)
            {
                decoded += static_cast<char>(current->symbol);
                if (decoded.size() == total_symbols)
                    return decoded;
                current = root;
            }
        }
    }

    return decoded;
}

#endif
