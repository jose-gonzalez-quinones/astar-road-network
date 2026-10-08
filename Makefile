CC = gcc
CFLAGS = -O2 -Wall -Wextra

all: build_graph astar

build_graph: src/build_graph.c
	$(CC) $(CFLAGS) -o $@ $<

astar: src/astar.c
	$(CC) $(CFLAGS) -o $@ $< -lm

clean:
	rm -f build_graph astar

.PHONY: all clean
