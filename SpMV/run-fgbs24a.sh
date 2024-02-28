#!/bin/bash

set -e

mkdir -p $(hostname)

(

echo "prim-benchmarks SpMV (dfatool fgbs24a edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

cd data/generate
./replicate ../bcsstk30.mtx 64 ../bcsstk30.mtx.64.mtx
cd ../..

for nr_dpus in 2543 2304 2048; do
	for nr_tasklets in 16; do
		echo
		if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets}; then
			# repetition is not part of upstream setup
			for i in `seq 1 100`; do
				timeout --foreground -k 1m 3m bin/host_code -v 0 -f data/bcsstk30.mtx.64.mtx || true
			done
		fi
	done
done
echo "Completed at $(date)"
) | tee "$(hostname)/fgbs24a.txt"

rm -f data/bcsstk30.mtx.64.mtx
