// Regression tests for the graph, both MST algorithms, and output files.
// Most tests use small graphs so an incorrect result is easy to trace.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Analysis.h"
#include "CostScenario.h"
#include "Graph.h"
#include "Kruskal.h"
#include "MapRender.h"
#include "MinHeap.h"
#include "Prim.h"
#include "UnionFind.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>

using Catch::Approx;

namespace {

// Small, hand-checkable graph used by many tests. It has a known minimum spanning
// tree: 0-2 (5), 1-2 (3), 1-3 (8), giving a total cost of 16.
Graph sampleGraph() {
    // Expected MST: A-C (5), C-B (3), B-D (8), total cost = 16.
    Graph graph(4);
    graph.addEdge(0, 1, 10);
    graph.addEdge(0, 2, 5);
    graph.addEdge(1, 2, 3);
    graph.addEdge(1, 3, 8);
    return graph;
}

// In addition to checking the reported cost, confirm the returned roads
// have no cycles and the expected number of edges for a forest.
void requireValidForest(const Graph& graph, const MSTResult& result) {
    // Union-find verifies the selected edges form a valid spanning forest without
    // creating cycles. We also recompute the total cost and length from the edges.
    UnionFind components(graph.getNumVertices());
    double calculatedCost = 0.0;
    double calculatedLength = 0.0;

    for (const Edge& road : result.edges) {
        REQUIRE(components.unite(road.source, road.destination));
        calculatedCost += road.cost;
        calculatedLength += road.length;
    }

    REQUIRE(result.totalCost == Approx(calculatedCost));
    REQUIRE(result.totalLengthMeters == Approx(calculatedLength));
    REQUIRE(result.edges.size() ==
            static_cast<std::size_t>(graph.getNumVertices() - result.components));
}

// Different MST edge sets are valid when multiple roads have equal costs.
// What matters is that the algorithms agree on the minimum total cost.
void requireSameMinimumCost(const Graph& graph) {
    // Run both algorithms against the same graph and verify they agree on the
    // minimum total cost and the number of connected components.
    MSTResult prim = Prim::computeMST(graph);
    MSTResult kruskal = Kruskal::computeMST(graph);

    REQUIRE(prim.totalCost == Approx(kruskal.totalCost));
    REQUIRE(prim.components == kruskal.components);
    requireValidForest(graph, prim);
    requireValidForest(graph, kruskal);
}

} // namespace

TEST_CASE("Graph stores each undirected road in both adjacency lists", "[graph]") {
    // This checks the core adjacency representation: each edge is stored in both
    // directions and invalid vertex access is rejected.
    Graph graph = sampleGraph();
    REQUIRE(graph.getNumVertices() == 4);
    REQUIRE(graph.getEdges().size() == 4);
    REQUIRE(graph.getNeighbors(1).size() == 3);
    REQUIRE(graph.getNeighbors(3).front().destination == 1);

    REQUIRE_THROWS_AS(graph.addEdge(-1, 0, 2), std::out_of_range);
    REQUIRE_THROWS_AS(graph.addEdge(0, 7, 2), std::out_of_range);
    REQUIRE_THROWS_AS(graph.addEdge(0, 1, -2), std::invalid_argument);
    REQUIRE_THROWS_AS(graph.getNeighbors(8), std::out_of_range);
}

TEST_CASE("Graph loading accepts short and extended edge records", "[graph][io]") {
    // Create a temporary file and exercise both supported file formats: the legacy
    // short form and the richer edge record with length and flood information.
    auto filename = std::filesystem::temp_directory_path() / "mapagator_graph_test.txt";

    {
        std::ofstream file(filename);
        file << "3 2\n0 1 7\n1 2 4\n";
    }
    Graph graph;
    graph.readFile(filename.string());
    REQUIRE(graph.getEdges().size() == 2);
    REQUIRE(graph.getEdges()[0].length == Approx(7));

    {
        std::ofstream file(filename);
        file << "3 1\n0 1 9 5 3 0.4\n";
    }
    graph.readFile(filename.string());
    REQUIRE(graph.getEdges().size() == 1);
    REQUIRE(graph.getEdges()[0].length == Approx(5));
    REQUIRE(graph.getEdges()[0].floodRisk == Approx(0.4));

    // Failed loads must not corrupt an already loaded graph.
    {
        std::ofstream file(filename);
        file << "3 2\n0 1 7\n";
    }
    REQUIRE_THROWS(graph.readFile(filename.string()));
    REQUIRE(graph.getEdges().size() == 1);
    // The temporary output streams have already gone out of scope.
    std::filesystem::remove(filename);
}

TEST_CASE("MinHeap removes the smallest edge first", "[heap]") {
    // A min-heap must deliver edges in nondecreasing order, including repeated values.
    MinHeap heap;
    REQUIRE(heap.empty());

    for (double cost : {9.0, 1.0, 8.0, 3.0, 3.0, 2.0}) {
        heap.insert(Edge(0, 1, cost));
    }
    for (double expected : {1.0, 2.0, 3.0, 3.0, 8.0, 9.0}) {
        REQUIRE(heap.extractMin().cost == Approx(expected));
    }
    REQUIRE(heap.empty());
    REQUIRE_THROWS(heap.extractMin());
}

TEST_CASE("UnionFind merges components and rejects cycles", "[unionfind]") {
    // This verifies the classic DSU behavior: merging creates connectivity, repeated
    // merges are rejected as cycles, and invalid indices throw exceptions.
    UnionFind groups(7);
    REQUIRE(groups.unite(0, 1));
    REQUIRE(groups.unite(1, 2));
    REQUIRE_FALSE(groups.unite(0, 2));
    REQUIRE(groups.connected(0, 2));
    REQUIRE_FALSE(groups.connected(0, 3));

    REQUIRE(groups.unite(3, 4));
    REQUIRE(groups.unite(2, 4));
    REQUIRE(groups.connected(0, 3));
    REQUIRE_THROWS(groups.find(7));
}

TEST_CASE("Prim and Kruskal find the sample MST with cost 16", "[mst]") {
    // This is the canonical sanity check: on a small known graph, both algorithms
    // should return the same minimum spanning tree total cost.
    Graph graph = sampleGraph();
    REQUIRE(Prim::computeMST(graph).totalCost == Approx(16));
    REQUIRE(Kruskal::computeMST(graph).totalCost == Approx(16));
    requireSameMinimumCost(graph);
}

TEST_CASE("Disconnected graph produces a minimum spanning forest", "[mst]") {
    // A disconnected graph should still produce a minimum spanning forest, with one
    // component for each connected part and isolated vertices counted as components.
    Graph graph(6);
    graph.addEdge(0, 1, 2);
    graph.addEdge(1, 2, 3);
    graph.addEdge(3, 4, 6);

    for (MSTResult result : {Prim::computeMST(graph), Kruskal::computeMST(graph)}) {
        REQUIRE(result.components == 3); // Includes isolated vertex 5.
        REQUIRE(result.edges.size() == 3);
        REQUIRE(result.totalCost == Approx(11));
        requireValidForest(graph, result);
    }
}

TEST_CASE("Algorithms handle self-loops, parallel roads, ties and empty graphs", "[mst]") {
    // This stress test covers edge cases: self-loops, duplicate roads, equal-weight
    // choices, and empty graphs should not break MST computation.
    Graph graph(4);
    graph.addEdge(0, 0, 0);
    graph.addEdge(0, 1, 9);
    graph.addEdge(0, 1, 1);
    graph.addEdge(1, 2, 1);
    graph.addEdge(0, 2, 1);

    REQUIRE(Prim::computeMST(graph).totalCost == Approx(2));
    REQUIRE(Kruskal::computeMST(graph).components == 2);
    requireSameMinimumCost(graph);

    Graph empty;
    REQUIRE(Prim::computeMST(empty).edges.empty());
    REQUIRE(Kruskal::computeMST(empty).components == 0);
    REQUIRE_THROWS(Prim::computeMST(Graph(1), 8));
}

TEST_CASE("Cost scenarios alter cost but preserve physical length", "[scenario]") {
    // Scenario transformation should modify the effective cost but not the underlying
    // road geometry or physical length field.
    Graph graph(2);
    graph.addEdge(0, 1, 50, 100, 1, 0.75);

    REQUIRE(applyScenario(graph, CostScenario::Length).getEdges()[0].cost == Approx(100));
    REQUIRE(applyScenario(graph, CostScenario::Fiber).getEdges()[0].cost == Approx(180));
    REQUIRE(applyScenario(graph, CostScenario::Flood).getEdges()[0].cost == Approx(175));
    REQUIRE(applyScenario(graph, CostScenario::Flood).getEdges()[0].length == Approx(100));
    REQUIRE_THROWS(parseScenario("other"));
}

TEST_CASE("Road closures and added roads change network connectivity", "[what-if]") {
    // These checks model real network edits: closing roads should increase cost or
    // disconnect the graph, while adding a hub road provides a shortcut.
    Graph graph(3);
    graph.addEdge(0, 1, 1);
    graph.addEdge(1, 2, 1);
    graph.addEdge(0, 2, 9);

    NetworkOptions options;
    options.closedEdges = {1};
    REQUIRE(analyze(graph, true, options).totalCost == Approx(10));
    REQUIRE(analyze(graph, false, options).totalCost == Approx(10));

    options.closedEdges = {1, 2};
    REQUIRE(analyze(graph, true, options).components == 2);

    options.hubEdges.emplace_back(0, 2, 2);
    REQUIRE(analyze(graph, false, options).totalCost == Approx(3));
}

TEST_CASE("Multiple anchors use a zero-cost virtual backbone", "[anchors]") {
    // Anchors should force a zero-cost virtual connection between selected vertices,
    // producing a cheaper MST through the backbone rather than via ordinary roads.
    Graph graph(4);
    graph.addEdge(0, 1, 7);
    graph.addEdge(1, 2, 10);
    graph.addEdge(2, 3, 4);

    NetworkOptions options;
    options.anchors = {0, 3};
    for (bool usePrim : {true, false}) {
        MSTResult result = analyze(graph, usePrim, options);
        REQUIRE(result.totalCost == Approx(11));
        REQUIRE(result.edges.size() == 2);
        for (const Edge& road : result.edges) {
            REQUIRE(road.source < 4);
            REQUIRE(road.destination < 4);
        }
    }
    options.anchors = {99};
    REQUIRE_THROWS(analyze(graph, true, options));
}

TEST_CASE("Prim and Kruskal agree on generated connected graphs", "[random][mst]") {
    // Randomized graphs catch subtle bugs in either implementation. The point is not
    // the exact edges but that both algorithms agree on the same minimal total cost.
    std::mt19937 random(112);
    for (int count : {5, 20, 100, 250}) {
        Graph graph(count);
        for (int vertex = 1; vertex < count; ++vertex) {
            graph.addEdge(vertex - 1, vertex, 1 + random() % 100);
        }
        for (int index = 0; index < 4 * count; ++index) {
            int source = random() % count;
            int destination = random() % count;
            if (source != destination) {
                graph.addEdge(source, destination, 1 + random() % 100);
            }
        }
        requireSameMinimumCost(graph);
    }
}

TEST_CASE("SVG export produces a map with highlighted road segments", "[render]") {
    // Exporting the map should produce a valid SVG file and include the selected MST
    // roads in a format the GUI frontend can consume.
    Graph graph = sampleGraph();
    for (int vertex = 0; vertex < 4; ++vertex) {
        graph.setVertex(vertex, 29.6 + vertex * 0.01, -82.3 - vertex * 0.01);
    }

    auto filename = std::filesystem::temp_directory_path() / "mapagator_map_test.svg";
    exportSVG(graph, Prim::computeMST(graph), filename.string());

    std::ifstream file(filename);
    std::string firstLine;
    std::getline(file, firstLine);
    REQUIRE(firstLine.find("<svg") != std::string::npos);
    file.close();  // Windows will not remove the SVG while it is still open.
    std::filesystem::remove(filename);
}
