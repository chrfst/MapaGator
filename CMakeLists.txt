cmake_minimum_required(VERSION 4.1)
project(MapaGator C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(MapaGator
        Graph.h
        Kruskal.h
        MinHeap.h
        Prim.h
        UnionFind.h
        Graph.h)
