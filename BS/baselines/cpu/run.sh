#!/bin/sh

set -e

HOST="$(hostname)"

echo $HOST

(
echo "prim-benchmarks BS CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

make -B verbose=1

for nr_threads in 88 64 44 32 24 20 1 2 4 6 8 12 16; do
	#for vs in 262144 524288 1048576 2097152; do
	# NMC also uses 262144 elements
	for vs in 262144; do
		for i in `seq 1 20`; do
			OMP_NUM_THREADS=${nr_threads} timeout --foreground -k 1m 30m ./bs_omp ${vs} 16777216 || true
			sleep 10
		done
	done
done
) | tee "${HOST}-explore.txt"
