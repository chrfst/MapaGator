#include "UnionFind.h"

#include <numeric>
#include <stdexcept>
#include <utility>

UnionFind::UnionFind(int elementCount) {
    if (elementCount < 0) { //negative size removal
        throw std::invalid_argument("UnionFind size cannot be negative");
    }
    parent.resize(elementCount);
    rank.resize(elementCount, 0);
    std::iota(parent.begin(), parent.end(), 0); //every element is its own set
}

int UnionFind::find(int element) {
    if (element < 0 || element >= static_cast<int>(parent.size())) {
        throw std::out_of_range("UnionFind element is out of range"); //bound check
    }

    
        if (parent[element] != element) {
            parent[element] = find(parent[element]);
        }
    return parent[element];
}
//if an element is its own parent its the root of the set
//if its not, keep following the parents up until we find ther root
//compress the path
bool UnionFind::unite(int first, int second) {
    int firstRoot = find(first);
    int secondRoot = find(second);
    if (firstRoot == secondRoot) return false; // already connected

    // attach the shorter tree under the taller tree
    if (rank[firstRoot] < rank[secondRoot]) {
        std::swap(firstRoot, secondRoot);
    }
    parent[secondRoot] = firstRoot;
    if (rank[firstRoot] == rank[secondRoot]) ++rank[firstRoot]; //if same height combine, otherwise shorter under taller
    return true;
}

bool UnionFind::connected(int first, int second) {
    return find(first) == find(second);
}
