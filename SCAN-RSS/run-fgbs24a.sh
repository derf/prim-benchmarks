#!/bin/bash

set -e

mkdir -p $(hostname)

(

echo "prim-benchmarks SCAN-RSS (dfatool fgbs24a edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# >2048 is not part of upstream
for nr_dpus in 2543 2304 2048; do
	for nr_tasklets in 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10; then
			timeout --foreground -k 1m 30m bin/host_code -w 0 -e 100 -i 251658240 -x 1 || true
		fi
	done
done
echo "Completed at $(date)"
) | tee "$(hostname)/fgbs24a.txt"