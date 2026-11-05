#pragma once
#include "Graph.h"
#include "MSTResult.h"

//Read each vectors adjacency list
//Add these to minHeap
//Add to MST
//Read newly connected vertices adjacency list
//Skip already connected vertices
//Repeat process
class Prim {
    public:
        static MSTResult prim(const Graph& graph, int startVertex = 0);
};