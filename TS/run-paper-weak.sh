#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i; ignored, always uses 262144 elements

(

echo "prim-benchmarks TS weak (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# 256 and 512 are not part of upstream
for nr_dpus in 512 256 1 4 16 64; do
	for nr_tasklets in 1 2 4 8 16; do
		echo
		# upstream code did not respect $BL in the makefile and used 256B (BL=8) instead.
		# BL=10 appears to be slightly faster.
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10; then
			i=$(( nr_dpus * 524288 ))
			timeout --foreground -k 1m 30m bin/ts_host -w 0 -e 100 -n $i || true
		fi
	done
done
) | tee log-paper-weak.txt
