#!/bin/sh

set -e

echo "prim-benchmarks BFS CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# default threads: 4

make
for nr_threads in 1 2 4 6 8 12 16 20 24 32; do
	OMP_NUM_THREADS=${nr_threads} timeout -k 1m 30m ./bfs -f ../../data/loc-gowalla_edges.txt || true
done
