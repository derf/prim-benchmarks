#!/bin/bash

set -e

(

echo "prim-benchmarks BFS strong-full (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_dpus in 256 512 1024 2048; do
	for nr_tasklets in 1 2 4 8 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} verbose=1; then
			# repetition is not part of upstream setup
			for i in `seq 1 50`; do
				timeout --foreground -k 1m 3m bin/host_code -f data/loc-gowalla_edges.txt || true
			done
		fi
	done
done
) | tee log-paper-strong-full.txt