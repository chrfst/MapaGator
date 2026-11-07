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
    //returns a MSTResult object containing the edges of the minimum spanning tree and its total weight
    public:
        static MSTResult computeMST(const Graph& graph, int startVertex = 0);
};