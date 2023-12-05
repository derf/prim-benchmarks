#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i: input size (number of elements, not number of bytes!)

(

echo "prim-benchmarks TRNS (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_dpus in 1 4 8 16 32 48 64; do
	for nr_tasklets in 8 12 16; do
		# 12288 run-paper-weak, run-paper-strong-full
		for i in 12288; do
			echo
			if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets}; then
				# upstream uses -p 2048 in strong-full, but then the number of DPUs is always constant...
				timeout --foreground -k 1m 90m bin/host_code -w 0 -e 40 -p 1 -o 12288 -x 0 || true
			fi
		done
	done
done

echo "Completed at $(date)"

) | tee "log-$(hostname)-rank.txt"
