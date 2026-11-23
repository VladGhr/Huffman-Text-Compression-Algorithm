#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>
#include "utils.h"

using namespace std;

// prints a byte in a readable way: printable ASCII as-is, everything else as hex
inline string printable_symbol(unsigned char c)
{
    if (c == ' ')
        return "(-space-)";
    if (c == '\n')
        return "(-new line-)";
    if (c >= 33 && c <= 126)
        return string(1, static_cast<char>(c));

    const char *hex = "0123456789ABCDEF";
    return string("0x") + hex[c >> 4] + hex[c & 0xF];
}

inline void print_huffman_tree(Node *root)
{
    if (root == nullptr)
    {
        cout << "Huffman tree is empty";
        return;
    }

    int level = 0;
    queue<Node *> q;

    q.push(root);

    while (!q.empty())
    {
        size_t n = q.size();

        cout << "Level " << level << " : ";

        for (size_t i = 0; i < n; i++)
        {
            Node *current_element = q.front();
            q.pop();

            if (current_element->is_leaf == false)
                cout << current_element->freq << " ";
            else
                cout << "(" << printable_symbol(current_element->symbol) << ") : " << current_element->freq << " ";

            if (current_element->left_child != nullptr)
                q.push(current_element->left_child);

            if (current_element->right_child != nullptr)
                q.push(current_element->right_child);
        }

        cout << endl;
        level++;
    }
}

inline void print_heap(priority_queue<Node *, vector<Node *>, Compare> min_heap)
{
    while (!min_heap.empty())
    {
        Node *current = min_heap.top();
        cout << printable_symbol(current->symbol) << " " << current->freq << endl;
        min_heap.pop();
    }
}

inline void print_freq_dict(const map<unsigned char, uint64_t> &freq_dict)
{
    for (const auto &p : freq_dict)
        cout << printable_symbol(p.first) << " " << p.second << endl;
}

inline void print_encoding_dict(const map<unsigned char, string> &encoding_dict)
{
    for (const auto &p : encoding_dict)
        cout << printable_symbol(p.first) << " " << p.second << endl;
}

#endif
