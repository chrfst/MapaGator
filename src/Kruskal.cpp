
#include "Kruskal.h"

#include "UnionFind.h"
#include <algorithm>
#include <chrono>

// Build a minimum spanning tree using Kruskal's algorithm.
// The graph is treated as an undirected network where each edge has a cost and length.
MSTResult Kruskal::computeMST(const Graph& graph) {
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();

    // Initialize the result object; the graph starts with one connected component
    // per vertex until the spanning tree is fully assembled.
    MSTResult result;
    const int vertexCount = graph.getNumVertices();
    result.components = vertexCount;
    if (vertexCount > 0) result.edges.reserve(vertexCount - 1);

    // Unlike the adjacency list, getEdges() stores each road only once.
    // Sorting by increasing cost ensures the cheapest safe edges are considered first.
    std::vector<Edge> sortedEdges = graph.getEdges();
    std::sort(sortedEdges.begin(), sortedEdges.end(),
              [](const Edge& a, const Edge& b) {
                  if (a.cost != b.cost) return a.cost < b.cost;
                  if (a.source != b.source) return a.source < b.source;
                  return a.destination < b.destination;
              });

    // UnionFind tracks which vertices are already connected.
    UnionFind components(vertexCount);
    for (const Edge& edge : sortedEdges) {
        // Skip edges that would create a cycle; only add edges that connect
        // previously separate components.
        if (!components.unite(edge.source, edge.destination)) continue;

        // Record the chosen edge and accumulate the spanning-tree metrics.
        result.edges.push_back(edge);
        result.totalCost += edge.cost;
        result.totalLengthMeters += edge.length;
        --result.components;

        // Once the tree connects all vertices, we can stop early.
        if (result.components == 1) break;
    }

    // Measure elapsed time for reporting and analysis output.
    result.runtimeMs = std::chrono::duration<double, std::milli>(
        Clock::now() - started).count();
    return result;
}
