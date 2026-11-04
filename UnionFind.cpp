#include <utility>
#include "UnionFind.h"
#include <numeric>
#include <stdexcept>

UnionFind::UnionFind(int n) : parent(n >= 0 ? n : 0), rank(n >= 0 ? n : 0, 0) {
    if (n < 0) throw std::invalid_argument("Negative element count");
    std::iota(parent.begin(), parent.end(), 0); //fill parent with 0..n-1 so every element is its own parent
}
int UnionFind::find(int element) {
    if (element < 0 || element >= static_cast<int>(parent.size()))
        throw std::out_of_range("Invalid UnionFind element");
    if (parent[element] != element) parent[element] = find(parent[element]); //if element isnt root, find it parent root then point this element at the root
    return parent[element]; //return root
}
bool UnionFind::unite(int first, int second) {
    int a = find(first), b = find(second); //look at root and compare
    if (a == b) return false; //already in same group
    if (rank[a] < rank[b]) std::swap(a, b); //makes a root of the taller tree
    parent[b] = a; //atach shorter tree
    if (rank[a] == rank[b]) ++rank[a];
    return true;
}
