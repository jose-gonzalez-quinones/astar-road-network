# A* Shortest Path on a Large-Scale Road Network

Implementation of the A* pathfinding algorithm on a large-scale road network covering Spain.

The project combines a C implementation for graph construction and A* search with a Python-based interactive visualization of the resulting route.

# Overview

The road network is represented as a directed graph constructed from a large CSV dataset. The graph is serialized into a binary representation for efficient access during pathfinding.

The A* implementation uses geographic distance calculated with the Haversine formula as both the edge cost and heuristic. A custom min-heap priority queue is used to manage the open set during the search.

The resulting route can be visualized on an interactive map using Python and Folium.

# Project structure
```text
astar-road-network/
├── data/
│   └── README.md
├── examples/
├── src/
│   ├── astar.c
│   └── build_graph.c
├── visualization/
│   └── map_plot.py
├── .gitignore
├── Makefile
├── requirements.txt
└── README.md
```

# Technical highlights

- Large-scale graph construction from road network data
- Directed graph representation with dynamically allocated adjacency lists
- Binary graph serialization
- A* shortest-path search
- Custom min-heap priority queue
- Haversine geographic distance
- Manual memory management in C
- Python-based interactive route visualization

# Requirements

## C

- GCC or another C compiler
- GNU Make

## Python

- Python 3
- pandas
- Folium

Python dependencies can be installed with:

```bash
pip install -r requirements.txt
```

# Build

Clone the repository and build the C programs with:

```bash
make
```

This produces:

build_graph
astar

To remove the compiled binaries:

```bash
make clean
```

# Dataset

The road network dataset is approximately 1.3 GB and is therefore not stored in this repository.

See [`data/README.md`](data/README.md) for download instructions.

After downloading the dataset, place it at:

data/spain.csv

# Usage

## 1. Build the graph

After placing the dataset at `data/spain.csv`, run:

```bash
./build_graph
```

This reads the road network and generates:

data/spain.bin

The default input and output paths can also be overridden:

```bash
./build_graph <input_csv> <output_binary>
```

## 2. Run A*

Run the A* search with the default start and destination nodes:

```bash
./astar
```

The default route is from Santa Maria del Mar in Barcelona to La Giralda in Seville.

The resulting route is written to:

examples/route.txt

Custom graph, start node, destination node and output paths can also be provided:

```bash
./astar <graph> <start_id> <goal_id> <output_file>
```

## 3. Visualize the route

Install the Python dependencies:

```bash
pip install -r requirements.txt
```

Then run:

```bash
python visualization/map_plot.py
```

This generates:

examples/route.html

The output can also be specified explicitly:

```bash
python visualization/map_plot.py <route_file>
```

## Example result

Using the included default node IDs, the A* search produced the following result on the full road network:

- Nodes in graph: 21,734,405
- Graph connections: 41,968,086
- Nodes expanded by A*: 5,329,287
- Route distance: 958.8 km
- Output: `examples/route.txt`

The generated route can be visualized as an interactive map using the Python visualization script.
