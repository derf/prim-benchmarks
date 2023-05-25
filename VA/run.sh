#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i: input size (number of elements, not number of bytes!)

echo "prim-benchmarks VA (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_dpus in 1 2 4 8 16 32 64 128 256 512; do
	for nr_tasklets in 1 2 3 4 6 8 10 12 16 20 24; do
		for i in  2048 4096 8192 16384 65536 262144 1048576 2621440; do
			for dt in CHAR SHORT INT32 INT64 FLOAT DOUBLE; do
				echo
				if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 TYPE=${dt}; then
					bin/host_code -w 0 -e 100 -i ${i} || true
				fi
			done
		done
	done
done
