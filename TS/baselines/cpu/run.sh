#!/bin/sh

set -e

HOST="$(hostname)"

echo $HOST

(

echo "prim-benchmarks TS CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# input size depends on file -> strong scaling only

make -B
for i in $(seq 1 10); do
	for nr_threads in 88 64 44 1 2 4 6 8 12 16 20 24 32; do
		OMP_NUM_THREADS=${nr_threads} timeout --foreground -k 1m 30m ./streamp_openmp inputs/randomlist33M.txt 256 || true
	done
done
) | tee "${HOST}-explore.txt"
