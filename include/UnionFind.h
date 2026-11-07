#pragma once

#include <vector>
class UnionFind {
public:
    explicit UnionFind(int elementCount);

    int find(int element);
    bool unite(int first, int second);
    bool connected(int first, int second);

private:
    std::vector<int> parent;
    std::vector<int> rank;
};
