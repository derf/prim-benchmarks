#!/bin/bash

set -e

mkdir -p $(hostname)

ts=$(date +%Y%m%d)

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i; ignored, always uses 262144 elements

(

echo "prim-benchmarks UNI (dfatool fgbs24a edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_dpus in 2304 2048 2543; do
	for nr_tasklets in 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10; then
			timeout --foreground -k 1m 30m bin/host_code -w 0 -e 100 -i 251658240 -x 1 || true
		fi
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
			timeout --foreground -k 1m 30m bin/host_code -w 0 -e 100 -i 251658240 -x 1 || true
		fi
	done
done
echo "Completed at $(date)"
) | tee "$(hostname)/${ts}-fgbs24a.txt"
