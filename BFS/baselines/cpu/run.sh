#!/bin/sh

set -e

echo "prim-benchmarks BFS CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# default threads: 4

# input size depends on file -> strong scaling only

make
for nr_threads in 1 2 4 6 8 12 16 20 24 32; do
	for f in loc-gowalla_edges roadNet-CA; do
		OMP_NUM_THREADS=${nr_threads} timeout --foreground -k 1m 30m ./bfs -f ../../data/${f}.txt || true
	done
done