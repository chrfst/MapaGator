// Command-line entry point for running and comparing the MST algorithms.
// The CLI also handles synthetic benchmarks and SVG/edge-list exports.

#include "Analysis.h"
#include "CostScenario.h"
#include "Graph.h"
#include "MapRender.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Arguments {
    std::string graphFile;
    std::string verticesFile;
    std::string algorithm = "both";
    std::string scenario = "length";
    std::string svgFile;
    std::string exportFile;
    NetworkOptions network;
    int startVertex = 0;
    int generatedVertices = 0;
    int generatedEdges = 0;
    bool benchmark = false;
    bool verify = true;
    bool help = false;
};

void printUsage() {
    std::cout
        << "MapaGator: minimum-cost road network\n\n"
        << "  --graph FILE                 Read a graph exported by preprocessing\n"
        << "  --vertices FILE              Read optional GPS coordinates\n"
        << "  --algorithm prim|kruskal|both (default both)\n"
        << "  --scenario length|fiber|flood (default length)\n"
        << "  --anchors 1,2,3              Zero-cost connection between anchor nodes\n"
        << "  --close EDGE_INDEX           Close one road; repeat for more\n"
        << "  --hub U,V,LENGTH             Add a hypothetical road (meters)\n"
        << "  --start VERTEX               Prim's starting vertex\n"
        << "  --svg FILE                   Export highlighted road map\n"
        << "  --export FILE                Export the chosen MST edges\n"
        << "  --generate VERTICES          Create a synthetic connected graph\n"
        << "  --density EDGES              Number of generated roads\n"
        << "  --benchmark                  Print timing scope\n"
        << "  --no-verify                  Skip comparing MST totals\n";
}

std::vector<int> parseIntegerList(const std::string& text) {
    // Parse a comma-separated list of integers such as "1,2,3".
    // This is used for anchor nodes and other graph-editing options.
    std::vector<int> values;
    std::stringstream input(text);
    std::string token;
    while (std::getline(input, token, ',')) {
        if (token.empty()) throw std::invalid_argument("Empty comma-separated value");
        std::size_t used = 0;
        int number = std::stoi(token, &used);
        if (used != token.size()) throw std::invalid_argument("Invalid number: " + token);
        values.push_back(number);
    }
    if (values.empty()) throw std::invalid_argument("Expected at least one number");
    return values;
}

// Keep argument handling here so the graph algorithms do not know
// anything about terminal flags or file paths.
Arguments parseArguments(int argc, char** argv) {
    // Parse the command-line options into a single configuration object.
    // This keeps the rest of the program focused on graph logic rather than CLI details.
    Arguments args;
    for (int index = 1; index < argc; ++index) {
        std::string option = argv[index];
        auto nextValue = [&]() -> std::string {
            if (++index >= argc) {
                throw std::invalid_argument("Missing value for " + option);
            }
            return argv[index];
        };

        if (option == "--help") args.help = true;
        else if (option == "--graph") args.graphFile = nextValue();
        else if (option == "--vertices") args.verticesFile = nextValue();
        else if (option == "--algorithm") args.algorithm = nextValue();
        else if (option == "--scenario") args.scenario = nextValue();
        else if (option == "--svg") args.svgFile = nextValue();
        else if (option == "--export") args.exportFile = nextValue();
        else if (option == "--start") args.startVertex = std::stoi(nextValue());
        else if (option == "--generate") args.generatedVertices = std::stoi(nextValue());
        else if (option == "--density") args.generatedEdges = std::stoi(nextValue());
        else if (option == "--anchors") args.network.anchors = parseIntegerList(nextValue());
        else if (option == "--close") {
            int edgeIndex = std::stoi(nextValue());
            if (edgeIndex < 0) throw std::invalid_argument("Road index cannot be negative");
            args.network.closedEdges.push_back(static_cast<std::size_t>(edgeIndex));
        } else if (option == "--hub") {
            auto parts = parseIntegerList(nextValue());
            if (parts.size() != 3) {
                throw std::invalid_argument("Hub format: U,V,LENGTH_METERS");
            }
            args.network.hubEdges.emplace_back(parts[0], parts[1], parts[2]);
        } else if (option == "--benchmark") args.benchmark = true;
        else if (option == "--no-verify") args.verify = false;
        else if (!option.empty() && option.front() != '-' && args.graphFile.empty()) {
            args.graphFile = option;
        } else {
            throw std::invalid_argument("Unknown option: " + option);
        }
    }

    if (!args.help && args.graphFile.empty() && args.generatedVertices == 0) {
        throw std::invalid_argument("Use --graph FILE or --generate VERTICES");
    }
    if (args.algorithm != "prim" && args.algorithm != "kruskal"
        && args.algorithm != "both") {
        throw std::invalid_argument("Algorithm must be prim, kruskal, or both");
    }
    return args;
}

// Used for comparisons without downloading OpenStreetMap. First make a
// connected backbone, then add extra randomly generated roads.
Graph generateGraph(int vertexCount, int edgeCount) {
    // Create a reproducible synthetic graph for testing and timing comparisons.
    // The initial chain ensures the result is connected before extra edges are added.
    if (vertexCount < 1) throw std::invalid_argument("Need at least one vertex");
    if (edgeCount == 0) edgeCount = 2 * vertexCount;
    if (edgeCount < vertexCount - 1) {
        throw std::invalid_argument("Connected graph needs at least V-1 edges");
    }

    Graph graph(vertexCount);
    std::mt19937 generator(112); // Fixed seed makes benchmark runs reproducible.
    auto randomCost = [&]() { return 1.0 + generator() % 1000; };

    for (int vertex = 1; vertex < vertexCount; ++vertex) {
        graph.addEdge(vertex - 1, vertex, randomCost());
    }
    for (int index = vertexCount - 1; index < edgeCount; ++index) {
        int source = generator() % vertexCount;
        int destination = generator() % vertexCount;
        if (source == destination && vertexCount > 1) {
            destination = (destination + 1) % vertexCount;
        }
        graph.addEdge(source, destination, randomCost());
    }
    return graph;
}

Graph loadGraph(const Arguments& args) {
    // Prefer the synthetic benchmark generator when testing, otherwise read
    // the exported graph file and optional vertex coordinate file.
    if (args.generatedVertices != 0) {
        return generateGraph(args.generatedVertices, args.generatedEdges);
    }
    Graph graph;
    graph.readFile(args.graphFile);
    if (!args.verticesFile.empty()) graph.readVertices(args.verticesFile);
    return graph;
}

void printResult(const std::string& name, const MSTResult& result) {
    // Display the aggregate information for one MST computation.
    std::cout << name << ": edges=" << result.edges.size()
              << " components=" << result.components
              << " cost=" << result.totalCost
              << " mileage=" << result.totalLengthMeters / 1609.344
              << " runtime_ms=" << result.runtimeMs << '\n';
}

// Tie edges may differ, but both methods should report the same total cost
// and number of connected components for a given graph.
void verifyAgreement(const MSTResult& prim, const MSTResult& kruskal) {
    const double scale = std::max({1.0, std::abs(prim.totalCost),
                                    std::abs(kruskal.totalCost)});
    if (std::abs(prim.totalCost - kruskal.totalCost) > 1e-9 * scale
        || prim.components != kruskal.components) {
        throw std::runtime_error("Prim and Kruskal produced different results");
    }
    std::cout << "Verification: matching total cost and components\n";
}

// Save the chosen roads as text for analysis outside the GUI.
void writeSelectedEdges(const MSTResult& result, const std::string& filename) {
    // Export the selected spanning-tree edges in a simple format that other tools can read.
    std::ofstream output(filename);
    if (!output) throw std::runtime_error("Cannot write result: " + filename);

    output << result.edges.size() << ' ' << result.totalCost << '\n';
    for (const Edge& road : result.edges) {
        output << road.source << ' ' << road.destination << ' '
               << road.cost << ' ' << road.length << '\n';
    }
}

} // namespace

// Keep setup and output reporting here; MST logic lives in the core library.
int main(int argc, char** argv) {
    // Entry point: parse the command line, build the working graph, then run the
    // selected MST algorithm(s) and report the resulting road network.
    try {
        Arguments args = parseArguments(argc, argv);
        if (args.help) {
            printUsage();
            return 0;
        }

        // Load the network once, then apply any scenario and operational constraints.
        Graph original = loadGraph(args);
        Graph weighted = applyScenario(original, parseScenario(args.scenario));
        Graph working = buildWorkingGraph(weighted, args.network);

        // Allow either algorithm to run independently or both together for verification.
        const bool runPrim = args.algorithm != "kruskal";
        const bool runKruskal = args.algorithm != "prim";
        MSTResult primResult, kruskalResult;
        if (runPrim) {
            primResult = analyze(weighted, true, args.network, args.startVertex);
        }
        if (runKruskal) {
            kruskalResult = analyze(weighted, false, args.network);
        }

        std::cout << std::fixed << std::setprecision(4);
        // Report the original network size and the selected-tree summary.
        std::cout << "Graph: vertices=" << original.getNumVertices()
                  << " edges=" << original.getEdges().size()
                  << " original_mileage=" << originalMileage(original) << '\n';
        if (runPrim) printResult("Prim", primResult);
        if (runKruskal) printResult("Kruskal", kruskalResult);
        if (runPrim && runKruskal && args.verify) {
            verifyAgreement(primResult, kruskalResult);
        }

        const MSTResult& selected = runPrim ? primResult : kruskalResult;
        double availableMiles = originalMileage(original);
        double selectedMiles = selected.totalLengthMeters / 1609.344;
        double percentage = availableMiles > 0.0
            ? 100.0 * selectedMiles / availableMiles : 0.0;
        std::cout << "Selected road mileage percentage=" << percentage << "%\n";

        if (!args.svgFile.empty()) exportSVG(working, selected, args.svgFile);
        if (!args.exportFile.empty()) writeSelectedEdges(selected, args.exportFile);
        if (args.benchmark) {
            std::cout << "Timing excludes file loading, cost changes, and rendering.\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "MapaGator error: " << error.what() << '\n';
        return 1;
    }
}
