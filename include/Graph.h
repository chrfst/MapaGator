#pragma once

#include <string>
#include <vector>

struct Vertex {
    int id = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    bool hasCoordinates = false;
};

struct Edge {
    int source;
    int destination;
    double cost;
    double length;
    int roadType;
    double floodRisk;

    Edge(int source, int destination, double cost,
         double length = -1.0, int roadType = 0, double floodRisk = 0.0);
};


class Graph {
public:
    explicit Graph(int vertexCount = 0);

    void addEdge(int source, int destination, double cost,
                 double length = -1.0, int roadType = 0, double floodRisk = 0.0);
    void setVertex(int id, double latitude, double longitude);

    void readFile(const std::string& filename);
    void readVertices(const std::string& filename);

    int getNumVertices() const;
    const std::vector<Vertex>& getVertices() const;
    const std::vector<Edge>& getEdges() const;
    const std::vector<Edge>& getNeighbors(int vertex) const;

private:
    std::vector<Vertex> vertices_;
    std::vector<Edge> edges_;
    std::vector<std::vector<Edge>> adjacencyList_;

    void checkVertex(int vertex) const;
};
