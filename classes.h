#include <iostream>
#include <vector>
#include <queue>

using namespace std;

class Node {
    public:
        int freq;
        char symbol;
        bool is_leaf;
        Node *left_child;
        Node *right_child;

    Node(char s, int f, bool leaf, Node *left = nullptr, Node *right = nullptr) {
        symbol = s;
        freq = f;
        is_leaf = leaf;
        left_child = left;
        right_child = right;
    };
};


class Huffman_Tree {
    public:
        Node *root;

    Huffman_Tree() {
        root = nullptr;
    };

    void build_huffman_tree(vector<pair<char, int>> v, int current){
        
        if(current == v.size()) {
            return;
        }

        if(v.size() == 0) {
            root = nullptr;
            return;
        }

        if(v.size() == 1) {

            char symbol = v[0].first;
            int freq = v[0].second;

            root = new Node(symbol, freq, false, nullptr, nullptr);
            return;
        }

        if(root == nullptr) {
            pair<char, int> first_element = v[current];
            pair<char, int> second_element = v[current + 1];

            Node *left_node = new Node(first_element.first, first_element.second, true, nullptr, nullptr);
            Node *right_node = new Node(second_element.first, second_element.second, true, nullptr, nullptr);

            int freq_sum = first_element.second + second_element.second;

            Node *new_root = new Node('\0', freq_sum, false, left_node, right_node);
            root = new_root;
        }

        else {
            int first_element = root->freq;
            pair<char, int> second_element = v[current];

            Node *right_node = new Node(second_element.first, second_element.second, true, nullptr, nullptr);

            int freq_sum = first_element + second_element.second;

            Node *new_root = new Node('\0',freq_sum, false, root, right_node);
            root = new_root;
        }

        build_huffman_tree(v, current + 1);
        
    };

    void print_huffman_tree() {

        if(root == nullptr) {
            cout<<"Huffman tree is empty";
            return;
        }

        if(root->left_child == nullptr && root->right_child == nullptr) {
            cout<<"Level 0: " << root->symbol << " : " << root->freq;
            return;
        }

        int level = 0;
        queue<Node*> q;

        q.push(root);

        while(q.empty() == 0) {
            int n = q.size();

            cout<<"Level "<< level << " : ";

            for(int i = 0; i < n; i++) {
                Node* current_element = q.front();
                q.pop();
                

                if(current_element->is_leaf == false)
                    cout<<current_element->freq << " ";
                
                else
                    cout<<current_element->symbol << " ";


                if(current_element->left_child != nullptr)
                    q.push(current_element->left_child);
                
                if(current_element->right_child != nullptr)
                    q.push(current_element->right_child);
            }

            cout<<endl;
            level++;
       }
    }
};