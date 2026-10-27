#ifndef CLASSES_H
#define CLASSES_H

#include <iostream>
#include <vector>
#include <queue>

using namespace std;

#define DUMMY '\0'

// heap custom comparator
class Node
{
    public:
    int freq;
    char symbol;
    bool is_leaf;
    Node *left_child;
    Node *right_child;
    
    Node(char s, int f, bool leaf, Node *left = nullptr, Node *right = nullptr)
    {
        symbol = s;
        freq = f;
        is_leaf = leaf;
        left_child = left;
        right_child = right;
    };
};

class Compare
{
public:
    bool operator()(Node *a, Node *b) const
    {
        return a->freq > b->freq;
    }
};

class Huffman_Tree
{
public:
    Node *root;

    Huffman_Tree()
    {
        root = nullptr;
    };

    void build_huffman_tree(priority_queue<Node *, vector<Node *>, Compare> &min_heap)
    {   
        //if the input text is a file made of only one char for example "aaaaaa",
        //the root is also a leaf
        bool only_one_char_in_text = true;

        // emtpy file
        if (min_heap.empty())
            return;

        //while there are at least two nodes in the heap, build the tree
        while (min_heap.size() > 1)
        {
            only_one_char_in_text = false;
            Node *left = min_heap.top();
            min_heap.pop();

            Node *right = min_heap.top();
            min_heap.pop();

            Node *parent = new Node(DUMMY, left->freq + right->freq, false, left, right);
            min_heap.push(parent);
        }

        //if there is a single node in the heap, the building process is over
        //if there was a single node in the heap from the beginning, its the new root
        root = min_heap.top();
        min_heap.pop();
        
        root->is_leaf = false;

        if(only_one_char_in_text == true) {
            root->is_leaf = true;
        }
    }
};

#endif