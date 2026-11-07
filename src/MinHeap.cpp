#include "MinHeap.h"

#include <stdexcept>
#include <utility>

bool MinHeap::comesBefore(const Edge& first, const Edge& second) {
    if (first.cost != second.cost) return first.cost < second.cost; //lower cost first
    if (first.source != second.source) return first.source < second.source; //tie break
    return first.destination < second.destination; //tie break 2
}

bool MinHeap::empty() const { return heap.empty(); }
int MinHeap::size() const { return static_cast<int>(heap.size()); }

void MinHeap::siftUp(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (!comesBefore(heap[index], heap[parent])) break; //already in order
        std::swap(heap[index], heap[parent]);
        index = parent;
    }
}

void MinHeap::siftDown(int index) {
    while (true) {
        int smallest = index;
        int left = 2 * index + 1;
        int right = left + 1;
        //find the smallest of parent and both children
        if (left < size() && comesBefore(heap[left], heap[smallest])) {
            smallest = left;
        }
        if (right < size() && comesBefore(heap[right], heap[smallest])) {
            smallest = right;
        }
        if (smallest == index) break; //already in order

        std::swap(heap[index], heap[smallest]);
        index = smallest;
    }
}

void MinHeap::insert(const Edge& edge) {
    heap.push_back(edge);
    siftUp(size() - 1);
}

Edge MinHeap::extractMin() {
    if (empty()) throw std::out_of_range("Cannot extract from an empty heap");

    Edge smallest = heap.front();
    heap.front() = heap.back();
    heap.pop_back();
    if (!empty()) siftDown(0);
    return smallest;
}