#pragma once
#include <vector>

class UnionFind {
private:
    std::vector<int> parent;
    std::vector<int> rank;
public:
    explicit UnionFind(int numElements);
    int find(int element);
    bool unite(int first, int second);
    bool unit(int first, int second) { return unite(first, second); }
    bool connected(int first, int second) { return find(first) == find(second); }
};
