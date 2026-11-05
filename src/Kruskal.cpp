
#include "Kruskal.h"
#include "UnionFind.h"
#include <algorithm>
#include <chrono>
#include <vector>

MSTResult Kruskal::computeMST(const Graph& graph) {
    using Clock = std::chrono::steady_clock;
    const auto begin = Clock::now();
    const int n = graph.getNumVertices();
    MSTResult result;
    result.components = n;
    if (n > 0) result.edges.reserve(static_cast<size_t>(n - 1));

    // Sort each undirected edge once; Graph::getEdges() is not the adjacency list.
    std::vector<Edge> sorted = graph.getEdges();
    std::sort(sorted.begin(), sorted.end(), [](const Edge& a, const Edge& b) {
        if (a.cost != b.cost) return a.cost < b.cost;
        if (a.source != b.source) return a.source < b.source;
        return a.destination < b.destination;
    });

    UnionFind sets(n);
    for (const Edge& edge : sorted) {
        if (sets.unite(edge.source, edge.destination)) {
            result.edges.push_back(edge);
            result.totalCost += edge.cost;
            --result.components;
            if (result.edges.size() == static_cast<size_t>(n - 1)) break;
        }
    }
    result.runtimeMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return result;
}
