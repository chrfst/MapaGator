#pragma once

#include "Graph.h"
#include <vector>
class MinHeap {
public:
    bool empty() const;
    int size() const;
    void insert(const Edge& edge);
    Edge extractMin();

private:
    std::vector<Edge> heap;
    void siftUp(int index);
    void siftDown(int index);
    static bool comesBefore(const Edge& first, const Edge& second);
};
