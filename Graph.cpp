
#include "Graph.h"
#include <fstream>
#include <iostream>

void Graph::readFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "File failed to open." << filepath << std::endl;
        return;
    }
    (file >> numVertices);
    int source;
    int destination;
    double cost;
    while (file >> source >> destination >> cost) {
        Edge edge(source, destination, cost);
        edges.push_back(edge);
        adjacencyList[source].push_back(edge);
    }
}
int Graph::getNumVertices() {

}
const std::vector<Edge>& Graph::getNeighbors (int vertex) const {

}
const std::vector<Edge>& Graph::getEdges() const {

}
