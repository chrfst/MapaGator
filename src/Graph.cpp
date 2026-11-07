#include "Graph.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

bool validNonnegative(double value) {
    return std::isfinite(value) && value >= 0.0; //reject bad values
}

bool validCoordinates(double latitude, double longitude) {
    return std::isfinite(latitude) && std::isfinite(longitude)
        && latitude >= -90.0 && latitude <= 90.0      //lat bounds
        && longitude >= -180.0 && longitude <= 180.0; //long bounds
}


Edge parseEdge(const std::string& line) {
    std::istringstream input(line);

    int source, destination, roadType = 0;
    double cost, length = -1.0, risk = 0.0; // if -1, length not given

    if (!(input >> source >> destination >> cost)) {
        throw std::runtime_error("Invalid road record: " + line);
    }

    std::string remainder;
    std::getline(input, remainder);

    if (!remainder.empty() && remainder.find_first_not_of(" \t\r") != std::string::npos) {
        std::istringstream extra(remainder);
        std::string unexpected;

        if (!(extra >> length >> roadType >> risk) || (extra >> unexpected)) {
            throw std::runtime_error("Expected 3 or 6 road columns: " + line);
        }
    }

    return Edge(source, destination, cost, length, roadType, risk);
}

}

Edge::Edge(int source, int destination, double cost,
           double length, int roadType, double floodRisk)
    : source(source),
      destination(destination),
      cost(cost),
      length(length == -1.0 ? cost : length), //default length is cost
      roadType(roadType),
      floodRisk(floodRisk) {}

Graph::Graph(int vertexCount) {
    if (vertexCount < 0) { //reject negative size
        throw std::invalid_argument("Vertex count cannot be negative");
    }

    vertices_.resize(vertexCount);
    adjacencyList_.resize(vertexCount);

    for (int id = 0; id < vertexCount; ++id) {
        vertices_[id].id = id; //set ids
    }
}

void Graph::checkVertex(int vertex) const { // bound checking
    if (vertex < 0 || vertex >= getNumVertices()) {
        throw std::out_of_range("Vertex ID is outside the graph");
    }
}

void Graph::addEdge(int source, int destination, double cost,
                    double length, int roadType, double floodRisk) {
    checkVertex(source);// both ends must exist
    checkVertex(destination);

    if (!validNonnegative(cost) || !validNonnegative(floodRisk) // reject poor values
        || (length != -1.0 && !validNonnegative(length))) {
        throw std::invalid_argument("Road cost, length, and risk must be nonnegative");
    }

    Edge road(source, destination, cost, length, roadType, floodRisk);
    edges_.push_back(road); // all edges list
    adjacencyList_[source].push_back(road);

    if (source != destination) { // skip self loops
        adjacencyList_[destination].emplace_back(
            destination, source, cost, length, roadType, floodRisk); // reverse edge
    }
}

void Graph::setVertex(int id, double latitude, double longitude) {
    checkVertex(id);
    if (!validCoordinates(latitude, longitude)) {
        throw std::invalid_argument("Invalid latitude or longitude");
    }
    vertices_[id] = {id, latitude, longitude, true}; // true = has coordinates
}

void Graph::readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open graph file: " + filename);
    }

    int vertexCount, edgeCount;
    std::string header; // read header
    if (!std::getline(file, header)) {
        throw std::runtime_error("Graph file is empty");
    }

    std::istringstream headerStream(header);
    std::string extra;
    if (!(headerStream >> vertexCount >> edgeCount)
        || vertexCount < 0 || edgeCount < 0 || (headerStream >> extra)) {
        throw std::runtime_error("Expected header: VERTEX_COUNT EDGE_COUNT");
    }

    Graph loaded(vertexCount); // temp graph for error handling
    for (int index = 0; index < edgeCount; ++index) {
        std::string line;
        if (!std::getline(file, line)) {
            throw std::runtime_error("Graph file ended before all roads were read");
        }
        const Edge road = parseEdge(line);
        loaded.addEdge(road.source, road.destination, road.cost, // addEdge validates
                       road.length, road.roadType, road.floodRisk);
    }

    // only blank lines allowed after edges
    std::string line;
    while (std::getline(file, line)) {
        if (line.find_first_not_of(" \t\r") != std::string::npos) {
            throw std::runtime_error("Unexpected extra road records");
        }
    }

    *this = std::move(loaded); // swap in new graph
}

void Graph::readVertices(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open vertices file: " + filename);
    }

    int count;
    if (!(file >> count) || count != getNumVertices()) { // count must match graph
        throw std::runtime_error("Vertex count does not match graph");
    }

    Graph updated = *this;
    std::vector<bool> seen(count, false); // catch repeats
    for (int index = 0; index < count; ++index) {
        int id;
        double latitude, longitude;
        if (!(file >> id >> latitude >> longitude)) {
            throw std::runtime_error("Invalid coordinate record");
        }

        updated.checkVertex(id); // check before using as index
        if (seen[id]) {
            throw std::runtime_error("Duplicate vertex ID in coordinate file");
        }
        seen[id] = true;
        updated.setVertex(id, latitude, longitude);
    }

    std::string trailing;
    if (file >> trailing) { // too many
        throw std::runtime_error("Unexpected extra vertex records");
    }
    *this = std::move(updated);
}

int Graph::getNumVertices() const {
    return static_cast<int>(vertices_.size());
}

// const references
const std::vector<Vertex>& Graph::getVertices() const { return vertices_; }
const std::vector<Edge>& Graph::getEdges() const { return edges_; }

const std::vector<Edge>& Graph::getNeighbors(int vertex) const {
    checkVertex(vertex);
    return adjacencyList_[vertex];
}