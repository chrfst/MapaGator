"""Convert OpenStreetMap roads into MapaGator's simple text graph format.

Requirements:
    pip install osmnx pyrosm geopandas

Examples:
    python file_preprocessing.py --download --places "Gainesville, Florida, USA"
    python file_preprocessing.py --pbf "C:/data/florida.osm.pbf"

Output:
    data/graph_data.txt  -> vertex count, edge count, then road records
    data/vertices.txt    -> vertex count, then coordinates

The C++ graph is undirected, even when OSM roads have one-way restrictions.
"""

import argparse
import csv
import math
from pathlib import Path

import osmnx as ox


# Type codes must match the multipliers in CostScenario.cpp.
ROAD_TYPES = {
    "motorway": 1,
    "trunk": 1,
    "primary": 2,
    "secondary": 3,
    "tertiary": 3,
    "residential": 4,
    "living_street": 4,
    "service": 5,
}


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--pbf", type=Path, help="Local Florida .osm.pbf file")
    source.add_argument("--download", action="store_true", help="Download data from OSM")

    parser.add_argument("--places", nargs="+", default=["Gainesville, Florida, USA"])
    parser.add_argument("--risk-csv", type=Path, help="CSV: osm_u,osm_v,risk")
    parser.add_argument("--out", type=Path, default=Path("data"))
    return parser.parse_args()


def load_region(args):
    boundaries = ox.geocode_to_gdf(args.places)
    region = boundaries.geometry.union_all()

    if args.download:
        # Keeping original intersections preserves the physical road endpoints.
        return ox.graph_from_polygon(region, network_type="drive", simplify=False)

    if not args.pbf.is_file():
        raise FileNotFoundError(f"PBF file not found: {args.pbf.resolve()}")

    from pyrosm import OSM

    reader = OSM(str(args.pbf.resolve()), bounding_box=list(region.bounds))
    nodes, roads = reader.get_network(nodes=True, network_type="driving")
    if roads.empty:
        raise RuntimeError("No roads found in the selected region")

    # Clip road records by geometry, then keep only referenced node records.
    roads = roads[roads.geometry.intersects(region)].copy()
    node_ids = set(roads["u"]) | set(roads["v"])
    nodes = nodes[nodes["id"].isin(node_ids)].copy()
    return reader.to_graph(nodes, roads, graph_type="networkx")


def load_risk_values(filename):
    if filename is None:
        return {}

    risk_by_road = {}
    with filename.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            start = int(row["osm_u"])
            end = int(row["osm_v"])
            risk = float(row["risk"])
            if not math.isfinite(risk) or risk < 0:
                raise ValueError("Flood risk must be finite and nonnegative")
            risk_by_road[tuple(sorted((start, end)))] = risk
    return risk_by_road


def road_type(highway):
    if isinstance(highway, list):
        highway = highway[0] if highway else "unknown"
    return ROAD_TYPES.get(str(highway), 0)


def road_identifier(data):
    identifier = data.get("osmid", data.get("id", "unknown"))
    if isinstance(identifier, list):
        return ",".join(sorted(str(part) for part in identifier))
    return str(identifier)


def collect_graph(network, risk_values):
    original_nodes = list(network.nodes(data=True))
    vertex_ids = {original_id: new_id for new_id, (original_id, _) in enumerate(original_nodes)}

    # Opposite-direction arcs of a road become one undirected road.
    # Distinct OSM ways can still produce parallel edges.
    unique_roads = {}
    for start, end, attributes in network.edges(data=True):
        if start == end or start not in vertex_ids or end not in vertex_ids:
            continue

        length = float(attributes.get("length") or 0.0)
        if not math.isfinite(length) or length <= 0:
            continue

        source, destination = sorted((vertex_ids[start], vertex_ids[end]))
        kind = road_type(attributes.get("highway", "unknown"))
        risk = risk_values.get(tuple(sorted((int(start), int(end)))), 0.0)
        key = (source, destination, road_identifier(attributes), round(length, 3), kind)
        unique_roads[key] = (source, destination, length, length, kind, risk)

    roads = [unique_roads[key] for key in sorted(unique_roads)]
    return original_nodes, vertex_ids, roads


def write_graph(output_dir, nodes, vertex_ids, roads):
    output_dir.mkdir(parents=True, exist_ok=True)

    graph_path = output_dir / "graph_data.txt"
    with graph_path.open("w", encoding="utf-8") as output:
        output.write(f"{len(vertex_ids)} {len(roads)}\n")
        for source, destination, cost, length, kind, risk in roads:
            output.write(
                f"{source} {destination} {cost:.6f} {length:.6f} {kind} {risk:.6f}\n"
            )

    vertices_path = output_dir / "vertices.txt"
    with vertices_path.open("w", encoding="utf-8") as output:
        output.write(f"{len(vertex_ids)}\n")
        for original_id, attributes in nodes:
            latitude = attributes.get("y", attributes.get("lat"))
            longitude = attributes.get("x", attributes.get("lon"))
            if latitude is None or longitude is None:
                raise ValueError(f"OSM node {original_id} has no coordinates")
            output.write(f"{vertex_ids[original_id]} {latitude:.9f} {longitude:.9f}\n")

    print(f"Exported {len(vertex_ids)} intersections and {len(roads)} roads")
    print(f"Graph: {graph_path.resolve()}")
    print(f"Vertices: {vertices_path.resolve()}")


def main():
    args = parse_arguments()
    network = load_region(args)
    risk_values = load_risk_values(args.risk_csv)
    nodes, vertex_ids, roads = collect_graph(network, risk_values)
    write_graph(args.out, nodes, vertex_ids, roads)

    if args.risk_csv is None:
        print("Flood risk is zero by default; provide --risk-csv for meaningful risk weights.")


if __name__ == "__main__":
    main()
