#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i; ignored, always uses 262144 elements

(

echo "prim-benchmarks TS strong-full (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# >2048 is not part of upstream
# 12 tasklets are not part of upstream (code does not work with 16…)
for nr_dpus in 2543 2304 256 512 1024 2048; do
	for nr_tasklets in 1 2 4 8 12 16; do
		echo
		# upstream code did not respect $BL in the makefile and used 256B (BL=8) instead.
		# This appears to be faster than BL=10.
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=8; then
			timeout --foreground -k 1m 30m bin/ts_host -w 0 -e 100 -n 33554432 || true
		fi
	done
done
) | tee log-paper-strong-full.txt
