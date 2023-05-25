#!/bin/bash

set -e

# -f: input file (i.e., input size)
# bin/host_code -f data/loc-gowalla_edges.txt

echo "prim-benchmarks BFS (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_dpus in 1 2 4 8 16 32 64 128 256 512; do
	for nr_tasklets in 1 2 3 4 6 8 10 12 16 20 24; do
		for f in loc-gowalla_edges roadNet-CA; do
			echo
			if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets}; then
				for i in `seq 1 20`; do
					timeout --foreground -k 1m 30m bin/host_code -f data/${f}.txt || true
				done
			fi
		done
	done
done
