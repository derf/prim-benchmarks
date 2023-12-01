#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i; ignored, always uses 262144 elements

(

echo "prim-benchmarks BS (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for i in 262144 16777216; do
	for nr_dpus in 1 4 8 16 32 64 128 256 512 768 1024 1536 2048 2304 2542; do
		for nr_tasklets in 8 12 16; do
			echo
			if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10; then
				timeout --foreground -k 1m 30m bin/bs_host -w 0 -e 100 -i $i || true
			fi
		done
	done
done
echo "Completed at $(date)"
) | tee "log-$(hostname)-ndpus.txt"
