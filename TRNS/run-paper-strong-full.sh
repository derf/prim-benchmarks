#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i; ignored, always uses 262144 elements

(

echo "prim-benchmarks TRNS strong-full (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# >2048 is not in upstream
for nr_dpus in 2543 2304 256 512 1024 2048; do
	for nr_tasklets in 1 2 4 8 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets}; then
			# upstream uses -p 2048, but then the number of DPUs is always constant...
			timeout --foreground -k 1m 90m bin/host_code -w 0 -e 40 -p $nr_dpus -o 12288 -x 1 || true
		fi
	done
done

echo "Completed at $(date)"

) | tee "log-$(hostname)-prim-strong-full.txt"
