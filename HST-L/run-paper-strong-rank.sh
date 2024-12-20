#!/bin/bash

set -e

(

echo "prim-benchmarks HST-S strong-rank (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# >64 are not part of upstream config space
for nr_dpus in 128 1 4 16 64; do
	for nr_tasklets in 1 2 4 8 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 verbose=1; then
			timeout --foreground -k 1m 30m bin/host_code -w 0 -e 100 -b 256 -x 1 || true
		fi
	done
done
) | tee log-paper-strong-rank.txt
