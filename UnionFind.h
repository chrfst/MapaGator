#pragma once
#include <vector>
struct UnionFind {
private:
    std::vector<int> parent;
    std::vector<int> rank;
public:
    UnionFind(int numElements);
    int find(int element);
    bool unit(int first, int second);
};