#pragma once
#include <vector>
#include <string>
struct Edge {
    int source; //vectorID of initial vector
    int destination; //vectorID of vector it links to
    double cost;
    Edge(int source, int destination, double cost) : source(source), destination(destination), cost(cost){}
};
class Graph {
private:
    int numVertices;
    std::vector<std::vector<Edge>> adjacencyList; //Inner vector stores edges, outer vector is our vertices
    std::vector<Edge> edges; //Stores edges separately
public:
    Graph(int numVertices);
	void readFile(const std::string& filepath);
    const std::vector<Edge>& getNeighbors (int vertex) const;
    const std::vector<Edge>& getEdges() const;
    int getNumVertices() const;
};

