#!/bin/bash

set -e

(

echo "prim-benchmarks WRAM microbenchmark (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for ndpu in 1 4 8 16; do
	for ntask in 1 2 4 8 12 16 20; do
		for bl in 4 5 6 7 8 9 10 11; do
			for op in streaming strided random; do
				if make -B NR_DPUS=$ndpu NR_TASKLETS=$ntask BL=$bl OP=$op; then
					bin/host_code -w 0 -e 10 || true
				fi
			done
		done
	done
done
echo "Completed at $(date)"
) | tee "log-$(hostname)-wram-only.txt"

(

echo "prim-benchmarks WRAM microbenchmark (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for ndpu in 1 4 8 16; do
	for ntask in 1 2 4 8 12 16 20; do
		for bl in 4 5 6 7 8 9 10 11; do
			for op in streaming strided random; do
				if make -B NR_DPUS=$ndpu NR_TASKLETS=$ntask BL=$bl OP=$op MEM=MRAM; then
					bin/host_code -w 0 -e 10 || true
				fi
			done
		done
	done
done
echo "Completed at $(date)"
) | tee "log-$(hostname)-wram-mram.txt"
