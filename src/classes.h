#ifndef CLASSES_H
#define CLASSES_H

#include <cstdint>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

using namespace std;

#define DUMMY 0

enum class EntryType : uint8_t {
    File    = 0,
    Dir     = 1,
    Symlink = 2,
    End     = 0xFF
};


class Node
{
public:
    uint64_t freq;
    unsigned char symbol;
    bool is_leaf;
    // unique id used to break ties between nodes with equal frequency,
    // so the tree is identical at compression and decompression time
    // regardless of the standard library implementation.
    // leaves: id = symbol (0..255), internal nodes: id = 256, 257, ...
    int id;
    Node *left_child;
    Node *right_child;

    Node(unsigned char s, uint64_t f, bool leaf, int node_id, Node *left = nullptr, Node *right = nullptr)
    {
        symbol = s;
        freq = f;
        is_leaf = leaf;
        id = node_id;
        left_child = left;
        right_child = right;
    };
};

// heap custom comparator (min-heap on freq, then on id)
class Compare
{
public:
    bool operator()(Node *a, Node *b) const
    {
        if (a->freq != b->freq)
            return a->freq > b->freq;
        return a->id > b->id;
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

    ~Huffman_Tree()
    {
        delete_subtree(root);
    }

    Huffman_Tree(const Huffman_Tree &) = delete;
    Huffman_Tree &operator=(const Huffman_Tree &) = delete;

    void build_huffman_tree(priority_queue<Node *, vector<Node *>, Compare> &min_heap)
    {
        // empty file
        if (min_heap.empty())
            return;

        int next_id = 256;

        // while there are at least two nodes in the heap, build the tree
        while (min_heap.size() > 1)
        {
            Node *left = min_heap.top();
            min_heap.pop();

            Node *right = min_heap.top();
            min_heap.pop();

            Node *parent = new Node(DUMMY, left->freq + right->freq, false, next_id++, left, right);
            min_heap.push(parent);
        }

        // the last node is the root
        // if the input has a single distinct byte (e.g. "aaaaaa"), the root is also a leaf
        root = min_heap.top();
        min_heap.pop();
    }

private:
    static void delete_subtree(Node *node)
    {
        if (node == nullptr)
            return;
        delete_subtree(node->left_child);
        delete_subtree(node->right_child);
        delete node;
    }
};

class GlobalHeader
{
    public:
        uint32_t magic;
        uint8_t version;
        uint32_t reserved;

        GlobalHeader(uint32_t magic, uint8_t version, uint32_t reserved) {
            this->magic = magic;
            this->version = version;
            this->reserved = reserved;
        }
};

class EntryHeader
{
    public:
        EntryType type;
        string path;          // relative to the archive root, UTF-8, '/' as separator
        uint32_t perms;
        int64_t mtime;        // seconds since the Unix epoch (signed: dates before 1970 are possible)
        uint64_t size;        // bytes of data: file size, 0 for Dir, target length for Symlink
        string link_target;   // only for Symlink: where the link points (UTF-8, '/'); empty otherwise

        EntryHeader(EntryType type, string path, uint32_t perms, int64_t mtime, uint64_t size,
                    string link_target = "") {
            this->type = type;
            this->path = path;
            this->perms = perms;
            this->mtime = mtime;
            this->size = size;
            this->link_target = link_target;
        }
};
#endif
