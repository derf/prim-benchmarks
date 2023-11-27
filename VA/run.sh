#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i: input size (number of elements, not number of bytes!)

if false; then
(

echo "prim-benchmarks VA (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_dpus in 8 64; do
	for nr_tasklets in 8 12 16 20 24; do
		# 2621440 run-paper-weak
		# 167772160 run-paper-strong-full (needs at least 20 DPUs)
		for i in 2048 4096 8192 16384 65536 262144 1048576 2621440 167772160; do
			for dt in CHAR SHORT INT32 INT64 FLOAT DOUBLE; do
				echo
				if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 TYPE=${dt} WITH_ALLOC_OVERHEAD=0 WITH_LOAD_OVERHEAD=0 WITH_FREE_OVERHEAD=0; then
					timeout --foreground -k 1m 30m bin/host_code -w 0 -e 50 -i ${i} -x 1 || true
				fi
			done
		done
	done
done
echo "Completed at $(date)"
) | tee "log-$(hostname)-ntasklets.txt"
fi

(

echo "prim-benchmarks VA (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# 2621440 run-paper-weak
# 167772160 run-paper-strong-full (needs at least 20 DPUs)
for i in 2048 4096 8192 16384 65536 262144 1048576 2621440 167772160; do
	for nr_dpus in 1 4 8 16 32 64 128 256 512 768 1024 1536 2048 2304 2542; do
		for nr_tasklets in 8 12 16; do
			for dt in CHAR SHORT INT32 INT64 FLOAT DOUBLE; do
				echo
				if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 TYPE=${dt} WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
					timeout --foreground -k 1m 30m bin/host_code -w 0 -e 50 -i ${i} -x 1 || true
				fi
			done
		done
	done
done
echo "Completed at $(date)"
) | tee "log-$(hostname)-ndpus.txt"
