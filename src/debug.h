#include <iostream>
#include "utils.h"

using namespace std;

void print_huffman_tree(Node *root)
{

    if (root == nullptr)
    {
        cout << "Huffman tree is empty";
        return;
    }

    if (root->left_child == nullptr && root->right_child == nullptr)
    {
        cout << "Level 0: " << root->symbol << " : " << root->freq;
        return;
    }

    int level = 0;
    queue<Node *> q;

    q.push(root);

    while (q.empty() == 0)
    {
        int n = q.size();

        cout << "Level " << level << " : ";

        for (int i = 0; i < n; i++)
        {
            Node *current_element = q.front();
            q.pop();

            if (current_element->is_leaf == false)
                cout << current_element->freq << " ";

            else
            {

                if (current_element->symbol == ' ')
                    cout << "(-space_symbol-) : " << current_element->freq << " ";
                else if (current_element->symbol == '\0')
                    cout << "(-string terminator-) : " << current_element->freq << " ";
                else if (current_element->symbol == '\n')
                    cout << "(-newe line-) : " << current_element->freq << " ";
                else
                    cout << "(" << current_element->symbol << ") : " << current_element->freq << " ";
            }

            if (current_element->left_child != nullptr)
                q.push(current_element->left_child);

            if (current_element->right_child != nullptr)
                q.push(current_element->right_child);
        }

        cout << endl;
        level++;
    }
}

void print_heap(priority_queue<Node *, vector<Node *>, Compare> min_heap) {

    while(!min_heap.empty()) {
        Node *current = min_heap.top();
        cout<<current->symbol << " " << current->freq << endl;
        min_heap.pop();
    }

}

void print_freq_dict(map<char, int> freq_dict) {

    for(pair<char, int> p : freq_dict) {
        cout<<p.first << " " << p.second << endl;
    }

}

void print_encoding_dict(map<char, string> encoding_dict) {

    for(pair<char, string> p : encoding_dict) {
        cout<<p.first << " " << p.second << endl;
    }

}