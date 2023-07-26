#!/bin/sh

set -e

HOST="$(hostname)"

echo $HOST

(

echo "prim-benchmarks SpMV CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# default threads: 4

# input size depends on file -> strong scaling only

make -B
for i in $(seq 1 50); do
	for nr_threads in 88 64 44 1 2 4 6 8 12 16 20 24 32; do
		OMP_NUM_THREADS=${nr_threads} timeout --foreground -k 1m 30m ./spmv -f ../../data/bcsstk30.mtx -v 0 || true
		sleep 10
	done
done
) | tee "${HOST}-explore.txt"
