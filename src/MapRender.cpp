#include "MapRender.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace {

// Minimal bounding box for the graph's coordinate system used to scale the SVG rendering.
struct Bounds {
    double south, north, west, east;
};

// Find the latitude/longitude extremes so the map can be normalized into the SVG canvas.
Bounds findBounds(const Graph& graph) {
    bool first = true;
    Bounds bounds{};

    for (const Vertex& vertex : graph.getVertices()) {
        if (!vertex.hasCoordinates) continue;
        if (first) {
            bounds = {vertex.latitude, vertex.latitude,
                      vertex.longitude, vertex.longitude};
            first = false;
        } else {
            bounds.south = std::min(bounds.south, vertex.latitude);
            bounds.north = std::max(bounds.north, vertex.latitude);
            bounds.west = std::min(bounds.west, vertex.longitude);
            bounds.east = std::max(bounds.east, vertex.longitude);
        }
    }
    if (first) throw std::runtime_error("SVG output requires vertex coordinates");
    return bounds;
}

} // namespace

// Export a simple SVG view of the full network and the computed minimum spanning tree.
void exportSVG(const Graph& graph, const MSTResult& mst,
               const std::string& filename) {
    constexpr double width = 1200.0;
    constexpr double height = 800.0;
    constexpr double margin = 25.0;

    // Normalize all coordinates to the canvas so the map fits within the SVG viewport.
    const Bounds bounds = findBounds(graph);
    const auto& vertices = graph.getVertices();

    auto screenX = [&](int id) {
        return margin + (vertices[id].longitude - bounds.west)
            / std::max(1e-12, bounds.east - bounds.west) * (width - 2 * margin);
    };
    auto screenY = [&](int id) {
        return height - margin - (vertices[id].latitude - bounds.south)
            / std::max(1e-12, bounds.north - bounds.south) * (height - 2 * margin);
    };

    std::ofstream out(filename);
    if (!out) throw std::runtime_error("Cannot create SVG: " + filename);

    // Draw a dark background and then overlay the graph edges and the MST in a brighter color.
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1200 800\">\n";
    out << "<rect width=\"1200\" height=\"800\" fill=\"#0c1725\"/>\n";

    auto drawRoad = [&](const Edge& road, const char* color, double width) {
        if (!vertices[road.source].hasCoordinates ||
            !vertices[road.destination].hasCoordinates) return;

        out << "<line x1=\"" << screenX(road.source)
            << "\" y1=\"" << screenY(road.source)
            << "\" x2=\"" << screenX(road.destination)
            << "\" y2=\"" << screenY(road.destination)
            << "\" stroke=\"" << color
            << "\" stroke-width=\"" << width << "\"/>\n";
    };

    for (const Edge& road : graph.getEdges()) drawRoad(road, "#485669", 0.6);
    for (const Edge& road : mst.edges) drawRoad(road, "#4ade80", 1.6);
    out << "</svg>\n";
}
