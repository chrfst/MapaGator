#include "MinHeap.h"
int minHeap::parent(int index) {
    return ((index-1)/2);
}
int minHeap::left(int index) {
    return ((2*index) + 1);
}
int minHeap::right(int index) {
    return ((2*index) + 2);
}
bool minHeap::empty() const {
    if (heap.size() == 0) {
        return true;
    }else {
        return false;
    }
}
int minHeap::size() const {
    return heap.size();
}
void minHeap::heapifyUp(int index) {
    while (index > 0 && heap[parent(index) < heap[index]) {
        int temp = heap[parent(index)];
        std::swap(heap[parent(index)], heap[index]);
        index = parent(index);
    }
}
void minHeap::heapifyDown(int index) {
    int size = heap.size();

    while (true) {
        int leftChild = left(index);
        int rightChild = right(index);
        int smallest = index;
        if (leftChild < size && heap[leftChild] < heap[smallest]) {
            smallest = leftChild;
        }
        if (rightChild < size && heap[rightChild] < heap[smallest]) {
            smallest = rightChild;
        }
        if (smallest != index) {
            std::swap(heap[index], heap[smallest]);
            index = smallest;
        } else {
            break;
        }
    }
}
void minHeap::insert(const Edge& edge) {
    heap.push_back(edge)
    heapifyUp(heap.size() - 1);
}
Edge minHeap::extractMin() {
    if (heap.size() == 0) {
        throw std::runtime_error("MinHeap is empty");
    }
    if (heap.size() == 1) {
        return heap.front();
    }
    Edge save = heap[0];
    heap[0] = heap.back();
    heap.pop_back();
    heapifyDown(0);
    return save;
}
