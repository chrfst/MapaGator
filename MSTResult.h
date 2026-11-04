#pragma once
#include "Graph.h"
#include <vector>

struct MSTResult {
    std::vector<Edge> mstEdges;
    double totalCost = 0.0;
    int components = 0;
    //double runTimeMs = 0.0; 
    //Core algorithm time 
};