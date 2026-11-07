#pragma once

#include "Graph.h"
#include "MSTResult.h"

#include <cstddef>
#include <vector>

struct NetworkOptions {
    std::vector<int> anchors; // vertices that must be connected to the network
    std::vector<std::size_t> closedEdges; // edges that are closed and cannot be used
    std::vector<Edge> hubEdges; // additional edges to connect hubs to the network
};

Graph buildWorkingGraph(const Graph& input, const NetworkOptions& options);
MSTResult analyze(const Graph& input, bool usePrim, const NetworkOptions& options, int startVertex = 0);
double originalMileage(const Graph& graph);