#!/bin/bash

set -e

mkdir -p $(hostname)

(

echo "prim-benchmarks HST-S (dfatool fgbs24a edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_dpus in 2543 2304 2048; do
	for nr_tasklets in 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10; then
			timeout --foreground -k 1m 30m bin/host_code -w 0 -e 100 -b 256 -x 2 || true
		fi
	done
done
echo "Completed at $(date)"
) | tee "$(hostname)/fgbs24a.txt"
