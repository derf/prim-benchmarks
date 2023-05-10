#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i: input size (number of elements, not number of bytes!)
# 2097152 B -> 2M is maximum for 64bit types (due to 16M per DPU)

for mem in MRAM WRAM; do
		for nr_dpus in 1 2 4 8 16 32 64 128 256 512; do
			for nr_tasklets in 1 2 3 4 6 8 10 12 16 20 24; do
	for op in copy copyw add scale triad; do
				for dt in uint8_t uint16_t uint32_t uint64_t float double; do
					if make -B MEM=${mem} OP=${op} NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 T=${dt} UNROLL=1 \
						|| make -B MEM=${mem} OP=${op} NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 T=${dt} UNROLL=0; then
						bin/host_code -w 0 -e 20 -i 2097152
					fi
				done
			done
		done
	done
done
