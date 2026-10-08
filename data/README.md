# Dataset

This project uses a large-scale road network dataset covering Spain.

The raw CSV dataset is approximately 1.3 GB in size and is not stored directly in this GitHub repository.

# Download

The complete dataset is available as a compressed ZIP archive:

[Download `spain.zip` from Dropbox](https://www.dropbox.com/scl/fi/fvoy47d93fz0m4jaitsur/spain.zip?rlkey=h1sqtmkakyvzcd429tzh1gbqu&st=b28t8llo&dl=0)

After downloading the archive, extract it and place the resulting CSV file at:

data/spain.csv

The expected filename is:

spain.csv

# Generated graph

The `build_graph` program processes the CSV dataset and generates the binary graph representation used by the A* implementation.

From the repository root, run:

```bash
./build_graph
```

This generates:

data/spain.bin

The generated binary graph is also excluded from version control because of its size.
