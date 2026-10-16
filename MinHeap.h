#pragma once
#include <vector>
class MinHeap {
private:
    std::vector<Edge> heap;
    int parent(int index) const;
    int left(int index) const;
    int right(int index) const;
public:
    MinHeap();
    void heapifyUp(int index);
    void heapifyDown(int index);
    bool empty() const;
    int size() const;
    void insert(const Edge& edge);
    Edge extractMin();
};