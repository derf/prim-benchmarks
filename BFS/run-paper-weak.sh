#!/bin/bash

set -e

(

echo "prim-benchmarks BFS weak (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# 256 and 512 are not part of upstream
for nr_dpus in 256 512 1 4 16 64; do
	for nr_tasklets in 1 2 4 8 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} verbose=1; then
			# repetition is not part of upstream setup
			for i in `seq 1 50`; do
				# upstream code uses some kind of generated rMat graphs, but does not provide instructions for reproduction
				timeout --foreground -k 1m 3m bin/host_code -f data/loc-gowalla_edges.txt || true
			done
		fi
	done
done |
) tee log-paper-weak.txt
