#include "Prim.h"
#include "MinHeap.h"
#include "Graph.h"
#include "MSTResult.h"
#include <chrono>
#include <stdexcept>
#include <vector>

MSTResult Prim::computeMST(const Graph& graph, int startVertex) {
    using Clock = std::chrono::steady_clock;
    const auto begin = Clock::now();
    const int n = graph.getNumVertices();
    if (n == 0) return {};
    if (startVertex < 0 || startVertex >= n)
        throw std::out_of_range("Invalid Prim start vertex");

    MSTResult result;
    result.edges.reserve(static_cast<size_t>(n - 1));
    std::vector<bool> visited(n, false);
    MinHeap candidates;

    // Also seed every unvisited vertex so disconnected graphs yield a forest.
    for (int seedIndex = 0; seedIndex < n; ++seedIndex) {
        const int seed = (seedIndex == 0) ? startVertex
                          : ((seedIndex <= startVertex) ? seedIndex - 1 : seedIndex);
        if (visited[seed]) continue;
        ++result.components;
        visited[seed] = true;
        for (const Edge& e : graph.getNeighbors(seed))
            if (!visited[e.destination]) candidates.insert(e);

        while (!candidates.empty()) {
            Edge best = candidates.extractMin();
            if (visited[best.destination]) continue; // Ignore stale / cycle edge.
            visited[best.destination] = true;
            result.edges.push_back(best);
            result.totalCost += best.cost;
            for (const Edge& e : graph.getNeighbors(best.destination))
                if (!visited[e.destination]) candidates.insert(e);
        }
    }
    result.runtimeMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return result;
}
