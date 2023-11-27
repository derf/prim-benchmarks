#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i: input size (number of elements, not number of bytes!)
# 2097152 B -> 2M is maximum for 64bit types (due to 16M per DPU)

(

echo "prim-benchmarks STREAM microbenchmark (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for i in 4096 16384 131072 1048576 2097152; do
	for nr_dpus in 1 4 8 16 32 64 128 256 512 768 1024 1536 2048 2304 2542; do
		for nr_tasklets in 8 12 16; do
			for dt in uint8_t uint16_t uint32_t uint64_t float double; do
				for op in copy copyw add scale triad; do
					echo
					if make -B OP=${op} NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 T=${dt} UNROLL=1 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1 \
						|| make -B OP=${op} NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 T=${dt} UNROLL=0 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
						timeout --foreground -k 1m 30m bin/host_code -w 0 -e 40 -i $i || true
					fi
				done
			done
		done
	done
done
echo "Completed at $(date)"
) | tee "log-$(hostname)-ndpus.txt"
