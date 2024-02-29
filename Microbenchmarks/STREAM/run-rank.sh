#!/bin/bash

set -e

echo "prim-benchmarks STREAM microbenchmark (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# -i: input size (number of elements, not number of bytes!)
# Each DPU uses three buffers, each of which holds $i * sizeof($dt) bytes.
# With a total MRAM capacity of 64M, this gives us ~21M per buffer, or 16M when rounding down to the next power of two.
# With a maximum data type width of 8B (uint64_t, double), this limits the number of elements per DPU to 2097152.
for dt in uint64_t uint32_t ; do #uint8_t uint16_t float double; do
	for i in 2097152 1048576 524288 131072 16384 4096; do
		for nr_dpus in 1 4 8 16 32 48 64; do
			for nr_tasklets in 1 8 12 16; do
				for op in triad scale add copy copyw; do
					# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
					# Our largest data type holds 8B, so the minimum block size is 3.
					# From a performance perspective, 8 to 10 is usually best for sequential operations.
					for bl in 3 8 10; do
						echo
						if make -B OP=${op} NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=${bl} T=${dt} UNROLL=1 WITH_ALLOC_OVERHEAD=0 WITH_LOAD_OVERHEAD=0 WITH_FREE_OVERHEAD=0 \
							|| make -B OP=${op} NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=${bl} T=${dt} UNROLL=0 WITH_ALLOC_OVERHEAD=0 WITH_LOAD_OVERHEAD=0 WITH_FREE_OVERHEAD=0; then
							# -w: number of un-timed warmup iterations
							# -e: number of timed iterations
							timeout --foreground -k 1m 30m bin/host_code -w 0 -e 100 -i $i -x 0 || true
						fi
					done
				done
			done
		done
	done
done
echo "Completed at $(date)"
