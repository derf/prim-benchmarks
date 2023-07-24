#!/bin/sh

set -e

HOST="$(hostname)"

echo $HOST

(

echo "prim-benchmarks BFS CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# default threads: 4

# input size depends on file -> strong scaling only
# each BFS invocation performs 100 iterations
# roadNet-CA appears to be too small; frequently gives inf throughput

make -B verbose=1
for nr_threads in 88 64 44 1 2 4 6 8 12 16 20 24 32; do
	for f in loc-gowalla_edges; do # roadNet-CA; do
		OMP_NUM_THREADS=${nr_threads} timeout --foreground -k 1m 30m ./bfs -f ../../data/${f}.txt || true
		sleep 10
	done
done
) | tee "${HOST}-explore.txt"
