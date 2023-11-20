#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i: input size (number of elements, not number of bytes!)

(

echo "prim-benchmarks UNI (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_dpus in 1 4 8 16 32 64 128 256 512 768 1024 1536 2048; do
	for nr_tasklets in 8 12 16 20 24; do
		# 3932160 run-paper-weak
		# 251658240 run-paper-strong-full
		# 1258291200 CPU baseline default
		for i in 2048 4096 8192 16384 65536 3932160 251658240 251658240; do
			echo
			if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
				timeout --foreground -k 1m 30m bin/host_code -w 0 -e 50 -i ${i} -x 1 || true
			fi
		done
	done
done
) | tee "log-$(hostname).txt"
