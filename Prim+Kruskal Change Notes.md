# MapaGator: MST implementation notes

## What changed
- Completed `Graph` with an undirected adjacency list, single-copy edge list, and strict file loader.
- Fixed the custom binary `MinHeap` and added `UnionFind` with path compression and union by rank.
- Implemented `Prim::computeMST` and `Kruskal::computeMST`, returning `MSTResult` (selected edges, total cost, component count, elapsed milliseconds).
- Added a small command-line driver and tests; corrected the CMake project to C++17.

## Build / test
```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/MapaGator path/to/graph_data.txt
```
On Windows with MSVC, executable may be in `build/Debug/MapaGator.exe`.

## Data format
First line: `<number_of_vertices> <number_of_edges>`.
Remaining lines: `<source_id> <destination_id> <nonnegative_finite_cost>`.
Vertex IDs must be in `[0, number_of_vertices-1]`.
Input is interpreted as an **undirected** road network. Each input edge is stored once in the master edge list but in both endpoints' adjacency lists.

## Algorithm behavior
- Prim uses lazy edge insertion into the team's custom MinHeap. Stale candidates are skipped. The first component begins at `startVertex`; remaining disconnected components are processed so the result is a *minimum spanning forest*. Complexity O(E log E) time for this lazy form, O(E+V) memory worst case.
- Kruskal sorts a copy of the master edge list with `std::sort` then invokes custom UnionFind. Complexity O(E log E) time, O(E+V) memory.
- Both return V - C selected edges for V vertices and C connected components; on a connected graph, C=1 and the result has V-1 edges.
- Total weights should agree, although individual selected edges can differ when costs are tied.

## Remaining work / integration caveats
1. The uploaded archive did not include actual OpenStreetMap `graph_data.txt`. Integration with the real dataset is not yet validated.
2. The Python exporter uses a driving NetworkX graph, which may have directed links and parallel arcs. Decide how to collapse OSM directed road segments into an undirected network, including duplicates and one-way roads, before considering results physically meaningful.
3. The exporter only writes three fields per edge (`source destination cost`). It does not preserve geographic vertex coordinates, road type, or separate physical length. Those are required for map rendering, fiber/flood cost scenarios, and mileage when cost != length. Extend the format / data model before those phases.
4. Zero-cost anchor semantics are **not implemented**. For a true multi-anchor connected-backbone problem, introducing a zero-cost virtual super-root attached to all anchors changes the optimization problem, and the virtual edges must be excluded from physical-road metrics. Simply starting Prim at one anchor does not implement the report's multi-anchor requirement.
5. No SFML, hub/road closure GUI, scenario switching, or memory benchmarking is included. These are subsequent phases.
6. Double weights are assumed nonnegative and finite, and the graph is interpreted as undirected, even if an OSM input edge was marked one-way. Discuss the domain model with your team.
7. The included `main.cpp` is a demonstration driver; integrate the algorithm classes into the GUI rather than treating this as final application UI.
