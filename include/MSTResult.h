#pragma once

#include "Graph.h"
#include <vector>

struct MSTResult {
    std::vector<Edge> edges;
    double totalCost = 0.0;
    double totalLengthMeters = 0.0;
    int components = 0;
    double runtimeMs = 0.0;
};
