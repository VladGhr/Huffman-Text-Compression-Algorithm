#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include </home/vladstefan/Personal/Huffman-Text-Compression-Algorithm/classes.h>

using namespace std;

map<char, int> create_freq_dict(string s) {
    map<char, int> freq_dict;

    for(char letter : s) {
        freq_dict[letter]++;
    }

    return freq_dict;
}

vector<pair<char, int>> build_huffman_vector(map<char, int>& freq_dict) {

    vector<pair<char,int>> sorted_freq_dict_elements(freq_dict.begin(), freq_dict.end());

    sort(sorted_freq_dict_elements.begin(), sorted_freq_dict_elements.end(), [](pair<char,int>& a, pair<char,int>& b) {
        return a.second < b.second;
    }); 

    return sorted_freq_dict_elements;
}

int main() {

    map<char, int> freq_dict;
    freq_dict = create_freq_dict("nigger");

    vector<pair<char, int>> v;

    v = build_huffman_vector(freq_dict);

    Huffman_Tree *huffman_tree = new Huffman_Tree();

    
    return 0;
}

