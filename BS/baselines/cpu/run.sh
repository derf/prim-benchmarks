#!/bin/sh

set -e

echo "prim-benchmarks BS CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

make

for nr_threads in 1 2 4 6 8 12 16 20 24 32; do
	for vs in 262144 524288 1048576 2097152; do
		for i in `seq 1 20`; do
			OMP_NUM_THREADS=${nr_threads} timeout --foreground -k 1m 30m ./bs_omp ${vs} 16777216 || true
		done
	done
done
