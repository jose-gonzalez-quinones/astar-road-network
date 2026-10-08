import sys
from pathlib import Path

import folium
import pandas as pd


def main() -> None:
    if len(sys.argv) >= 2:
        input_file = Path(sys.argv[1])
    else:
        input_file = Path(__file__).parents[1] / "examples" / "route.txt"

    if not input_file.exists():
        raise FileNotFoundError(f"Input file not found: {input_file}")

    output_file = input_file.with_suffix(".html")

    nodes = pd.read_csv(
        input_file,
        delimiter="|",
        skiprows=3,
        header=None,
        names=["id_raw", "latitude", "longitude", "dist_raw"],
    )

    nodes["node_id"] = nodes["id_raw"].map(
        lambda value: int(str(value).split("=")[1].strip())
    )
    nodes["distance_from_start"] = nodes["dist_raw"].map(
        lambda value: float(str(value).split("=")[1].strip())
    )

    nodes["latitude"] = nodes["latitude"].astype(float)
    nodes["longitude"] = nodes["longitude"].astype(float)

    coordinates = nodes[["latitude", "longitude"]].values.tolist()

    center = [
        nodes["latitude"].mean(),
        nodes["longitude"].mean(),
    ]

    route_map = folium.Map(
    location=center,
    tiles="Esri.WorldStreetMap",
    zoom_start=7,
)

    folium.PolyLine(
        coordinates,
        weight=4,
        opacity=0.8,
    ).add_to(route_map)

    folium.Marker(
        coordinates[0],
        popup=f"Start — node {nodes.iloc[0]['node_id']}",
    ).add_to(route_map)

    folium.Marker(
        coordinates[-1],
        popup=f"Destination — node {nodes.iloc[-1]['node_id']}",
    ).add_to(route_map)

    route_map.save(output_file)

    print(f"Map saved to {output_file}")


if __name__ == "__main__":
    main()