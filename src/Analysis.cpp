#include "Analysis.h"

#include "Kruskal.h"
#include "Prim.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace {
constexpr double METERS_PER_MILE = 1609.344;
}

// Build a working copy of the graph that incorporates user-driven edits such as
// closing roads, adding optional hub edges, and connecting anchor nodes to a virtual root.
Graph buildWorkingGraph(const Graph& input, const NetworkOptions& options) {
    const int vertexCount = input.getNumVertices();
    const bool hasAnchors = !options.anchors.empty();
    Graph working(vertexCount + (hasAnchors ? 1 : 0));

    // Preserve existing vertex metadata such as coordinates for rendering and analysis.
    for (const Vertex& vertex : input.getVertices()) {
        if (vertex.hasCoordinates) {
            working.setVertex(vertex.id, vertex.latitude, vertex.longitude);
        }
    }

    // Validate any closed-road indices before deleting them from the working graph.
    std::unordered_set<std::size_t> closed(
        options.closedEdges.begin(), options.closedEdges.end());
    for (std::size_t index : closed) {
        if (index >= input.getEdges().size()) {
            throw std::out_of_range("Closed road index does not exist");
        }
    }

    // Copy all active roads into the working graph, skipping the roads the user chose to close.
    for (std::size_t index = 0; index < input.getEdges().size(); ++index) {
        if (closed.count(index)) continue;
        const Edge& road = input.getEdges()[index];
        working.addEdge(road.source, road.destination, road.cost,
                        road.length, road.roadType, road.floodRisk);
    }

    // Add any extra hub connections requested by the user.
    for (const Edge& road : options.hubEdges) {
        working.addEdge(road.source, road.destination, road.cost,
                        road.length, road.roadType, road.floodRisk);
    }

    // A virtual root joins all anchors at zero cost. These synthetic links
    // are removed from the physical road result after MST computation.
    for (int anchor : options.anchors) {
        if (anchor < 0 || anchor >= vertexCount) {
            throw std::out_of_range("Anchor vertex does not exist");
        }
        working.addEdge(vertexCount, anchor, 0.0, 0.0);
    }
    return working;
}

// Run the chosen MST algorithm on the transformed graph and strip out the synthetic
// anchor-root edges before returning the final road network.
MSTResult analyze(const Graph& input, bool usePrim,
                  const NetworkOptions& options, int startVertex) {
    Graph working = buildWorkingGraph(input, options);
    const int virtualRoot = input.getNumVertices();
    const bool hasAnchors = !options.anchors.empty();
    const int root = hasAnchors ? virtualRoot : startVertex;

    MSTResult result = usePrim
        ? Prim::computeMST(working, root)
        : Kruskal::computeMST(working);

    if (hasAnchors) {
        // Remove the artificial zero-cost anchor links; they exist only to force the
        // MST to include all requested anchor points before we return the real network.
        auto isVirtual = [virtualRoot](const Edge& edge) {
            return edge.source == virtualRoot || edge.destination == virtualRoot;
        };
        result.edges.erase(
            std::remove_if(result.edges.begin(), result.edges.end(), isVirtual),
            result.edges.end());
    }
    return result;
}

// Convert the total physical road length into miles for reporting.
double originalMileage(const Graph& graph) {
    double meters = 0.0;
    for (const Edge& road : graph.getEdges()) {
        if (road.source != road.destination) meters += road.length;
    }
    return meters / METERS_PER_MILE;
}

