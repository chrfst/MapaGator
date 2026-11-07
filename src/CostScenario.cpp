#include "CostScenario.h"

#include <stdexcept>

// Translate a string name into one of the supported cost models for scenario analysis.
CostScenario parseScenario(const std::string& name) {
    if (name == "length") return CostScenario::Length;
    if (name == "fiber") return CostScenario::Fiber;
    if (name == "flood") return CostScenario::Flood;
    throw std::invalid_argument("Scenario must be length, fiber, or flood");
}

// Compute the effective cost for a single road under the selected scenario.
double scenarioCost(const Edge& edge, CostScenario scenario) {
    if (scenario == CostScenario::Length) return edge.length;
    if (scenario == CostScenario::Flood) {
        // Flood risk increases effective cost by a percentage of the road length.
        return edge.length * (1.0 + edge.floodRisk);
    }

    // Illustrative fiber installation multipliers, NOT surveyed project costs.
    // Road type: 0 other, 1 motorway, 2 primary, 3 secondary,
    //            4 residential, 5 service.
    constexpr double multipliers[] = {1.0, 1.8, 1.4, 1.2, 1.0, 1.1};
    int roadType = edge.roadType;
    if (roadType < 0 || roadType > 5) roadType = 0;
    return edge.length * multipliers[roadType];
}

// Create a new graph whose edge weights reflect the chosen cost scenario while
// preserving the original geometry and metadata for the vertices.
Graph applyScenario(const Graph& graph, CostScenario scenario) {
    Graph weighted(graph.getNumVertices());

    for (const Vertex& vertex : graph.getVertices()) {
        if (vertex.hasCoordinates) {
            weighted.setVertex(vertex.id, vertex.latitude, vertex.longitude);
        }
    }

    for (const Edge& edge : graph.getEdges()) {
        weighted.addEdge(edge.source, edge.destination,
                         scenarioCost(edge, scenario), edge.length,
                         edge.roadType, edge.floodRisk);
    }
    return weighted;
}
