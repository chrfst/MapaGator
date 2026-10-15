#pragma once
class MinHeap {
private:
    std::vector<Edge> heap;
    int parent(int index) const;
    int left(int index) const;
    int right(int index) const;
public:
    MinHeap();
    bool empty() const;
    int size() const;
    void insert(const Edge& edge);
    Edge extractMin();
};