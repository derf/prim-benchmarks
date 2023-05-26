#!/bin/sh

set -e

echo "prim-benchmarks MLP CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

make

for nr_threads in 1 2 4 6 8 12 16 20 24 32; do
	OMP_NUM_THREADS=${nr_threads} timeout --foreground -k 1m 30m ./mlp_openmp || true
done
