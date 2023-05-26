#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -m: number of rows
# -n: number of cols

echo "prim-benchmarks VA (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for n in 512 1024 2048 4096; do
	for m in 512 1024 2048 4096; do
		for nr_dpus in 1 2 4 8 16 32 64 128 256 512; do
			for nr_tasklets in 1 2 3 4 6 8 10 12 16 20 24; do
				echo
				if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10; then
					timeout --foreground -k 1m 30m bin/gemv_host -w 0 -e 100 -m $m -n $n || true
				fi
			done
		done
	done
done
