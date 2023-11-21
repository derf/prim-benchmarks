#!/bin/bash

set -e

# BL: use 2^(BL) B blocks for MRAM <-> WRAM transfers on PIM module
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -m: number of rows
# -n: number of cols

(

echo "prim-benchmarks GEMV (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# run-paper-strong-full: m=163840 n=4096
# run-paper-strong-rank: m=8192 n=1024
# run-paper-weak: m=ndpus*1024 n=2048
for n in 512 1024 2048 4096; do
	for m in 512 1024 2048 4096 8192 163840; do
		for nr_dpus in 1 4 8 16 32 64 128 256 512 768 1024 1536 2048; do
			for nr_tasklets in 8 12 16; do
				echo
				if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10; then
					timeout --foreground -k 1m 30m bin/gemv_host -w 0 -e 100 -m $m -n $n || true
				fi
			done
		done
	done
done
) | tee "log-$(hostname)-ndpus.txt"
