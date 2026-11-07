#pragma once

#include "Graph.h"
#include <string>

enum class CostScenario { Length, Fiber, Flood };

CostScenario parseScenario(const std::string& name);
double scenarioCost(const Edge& edge, CostScenario scenario);
Graph applyScenario(const Graph& graph, CostScenario scenario);
