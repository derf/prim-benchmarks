#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i; ignored, always uses 262144 elements

echo "prim-benchmarks VA strong-rank (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# 256 and 512 are not part of upstream config spac
for nr_dpus in 1 4 16 64 256 512; do
	for nr_tasklets in 1 2 4 8 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10; then
			timeout --foreground -k 1m 30m bin/host_code -w 0 -e 100 -i 2621440 -x 1 || true
		fi
	done
done | tee log-paper-strong-rank.txt