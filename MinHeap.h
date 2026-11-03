#pragma once
#include <vector>
#include "Graph.h"
class minHeap {
private:
    std::vector<Edge> heap;
    int parent(int index) const;
    int left(int index) const;
    int right(int index) const;
public:
    minHeap();
    void heapifyUp(int index);
    void heapifyDown(int index);
    bool empty() const;
    int size() const;
    void insert(const Edge& edge);
    Edge extractMin();
};