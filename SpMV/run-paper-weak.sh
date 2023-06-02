#!/bin/bash

set -e

(

echo "prim-benchmarks SpMV weak (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# 256 and 512 are not part of upstream
for nr_dpus in 256 512 1 4 16 64; do
	cd data/generate
	make
	./replicate ../bcsstk30.mtx ${nr_dpus} /tmp/bcsstk30.mtx.${nr_dpus}.mtx
	cd ../..
	for nr_tasklets in 1 2 4 8 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} verbose=1; then
			# repetition is not part of upstream setup
			for i in `seq 1 50`; do
				timeout --foreground -k 1m 3m bin/host_code -v 0 -f /tmp/bcsstk30.mtx.${nr_dpus}.mtx || true
			done
		fi
	done
	rm -f /tmp/bcsstk30.mtx.${nr_dpus}.mtx
done |
) tee log-paper-weak.txt
