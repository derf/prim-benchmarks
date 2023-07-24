#!/bin/bash

set -e

(

echo "prim-benchmarks GEMV strong-full (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# >2048 are not part of upstream
for nr_dpus in 2543 2304 256 512 1024 2048; do
	for nr_tasklets in 1 2 4 8 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 verbose=1; then
			timeout --foreground -k 1m 30m bin/gemv_host -w 1 -e 100 -m 163840 -n 4096 || true
		fi
	done
done
) | tee log-paper-strong-full.txt
