import osmnx as ox
from pyrosm import OSM
import pandas as pd
#pyrosm is used to take in our pbf
pbf_path = "florida-261019.osm.pbf"
osm = OSM(pbf_path)
# 2. Extract only driving roads
nodes, edges = osm.get_network(nodes=True, network_type="driving")
G_state = osm.to_graph(nodes, edges, graph_type="networkx")
#narrow down to our select counties & combine them into single boundary
places = ["Alachua County, FL", "Marion County, FL", "Duval County, FL", "Hillsborough County, FL"]
boundaries = ox.geocode_to_gdf(places)
combined_boundary = boundaries.union_all()
G = ox.truncate.truncate_graph_polygon(G_state, combined_boundary)
#simplify (lowkey dont 100% understand what it does but it seems it could help)
#G = ox.simplify_graph(G)
print(G.number_of_nodes())
#Map OSM node IDs to sequential 0-indexed integers (so we dont have insanely long id's)
node_mapping = {old_id: new_id for new_id, old_id in enumerate(G.nodes())}
#Extract edges with their attributes
edges_data = []
for u, v, k, data in G.edges(keys=True, data=True):
    # Map the node IDs
    src = node_mapping[u] #source
    dst = node_mapping[v] #distance
    # Get the length of the road
    # This serves as the "cost" weight for Prim/Kruskal -- we will change this for different cost scenarios
    weight = data.get('length', 0.0)
    edges_data.append([src, dst, weight]) #functionally, start intersection, end intersection, and weight/distance
#Save to space-separated file (no comma separation this should make it easier
# The first row will contain total nodes & edges
num_nodes = G.number_of_nodes()
num_edges = len(edges_data)
with open("graph_data.txt", "w") as f:
    # header
    f.write(f"{num_nodes} {num_edges}\n")
    # edge - source destination weight
    for edge in edges_data:
        f.write(f"{edge[0]} {edge[1]} {edge[2]:.2f}\n")
