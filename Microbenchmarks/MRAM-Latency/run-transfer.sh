#!/bin/bash

# The DPU application reads blocks of data from MRAM to WRAM via mram_read
# and copies blocks of data from WRAM to MRAM via mram_write.
# Each transfer handles BLOCK_SIZE (BL<<2) bytes via DMA.
# Each DPU reports its total runtime, i.e., the number of cycles accumulated
# over all mram_read or mram_write calls.
# Total data size is 8192 uint64 elements, so 65536 B (64 KiB)
# Overall throughput is 65536 B / dpu_seconds
# Per-DPU throughput is 65536 B / (dpu_seconds * nr_tasklets)
# Latency per mram_* call is dpu_seconds / dpu_count

set -e

(

echo "prim-benchmarks MRAM microbenchmark (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for ndpu in 1 4 8 16 32 48 64; do
	for ntask in 1 2 4 8 12 16 20; do
		for bl in 4 5 6 7 8 9 10 11; do
			if make -B NR_DPUS=$ndpu NR_TASKLETS=$ntask BL=$bl; then
				bin/host_code -w 0 -e 100 || true
			fi
		done
	done
done
echo "Completed at $(date)"
) | tee "log-$(hostname).txt"
xz -v -9 -M 800M "log-$(hostname).txt"
