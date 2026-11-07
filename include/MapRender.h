#pragma once

#include "Graph.h"
#include "MSTResult.h"
#include <string>

// Export the current road network and the computed MST to an SVG file so the
// network can be visualized in a browser or external mapping tool.
void exportSVG(const Graph& graph, const MSTResult& mst,
               const std::string& filename);
