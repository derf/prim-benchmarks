#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i; ignored, always uses 262144 elements

(

echo "prim-benchmarks BS strong-rank (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# 256 and 512 are not part of upstream
for nr_dpus in 512 256 1 4 16 64; do
	for nr_tasklets in 1 2 4 8 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets}; then
			# upstream uses -p 64, but then the number of DPUs is always constant...
			timeout --foreground -k 1m 60m bin/host_code -w 0 -e 100 -p $nr_dpus -o 12288 -x 1 || true
		fi
	done
done
)| tee log-paper-strong-rank.txt