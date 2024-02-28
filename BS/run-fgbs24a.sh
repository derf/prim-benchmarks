#!/bin/bash

set -e

mkdir -p $(hostname)

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i; ignored, always uses 262144 elements

(

echo "prim-benchmarks BS (dfatool fgbs24a edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_dpus in 2543 2304 2048; do
	for nr_tasklets in 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10; then
			timeout --foreground -k 1m 30m bin/bs_host -w 0 -e 100 -i 16777216 || true
		fi
	done
done
echo "Completed at $(date)"
) | tee "$(hostname)/fgbs24a.txt"
