#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations

(

echo "prim-benchmarks RED strong-full (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# >2048 are not part of upstream
for nr_dpus in 2543 2304 256 512 1024 2048; do
	for nr_tasklets in 1 2 4 8 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 VERSION=SINGLE verbose=1; then
			timeout --foreground -k 1m 30m bin/host_code -w 0 -e 100 -i 419430400 -x 1 || true
		fi
	done
done
) | tee log-paper-strong-full.txt