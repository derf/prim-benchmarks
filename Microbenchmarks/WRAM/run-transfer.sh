#!/bin/bash

# The DPU application reads BL<<2 sized blocks of data from MRAM to WRAM via
# DMA (mram_read). It then performs the specified type of copy operation
# (i.e., streaming copy / strided copy / copy of random elements) within this
# block and writes the result back from WRAM to MRAM via DMA (mram_write).
# By default (MEM=WRAM), each tasklet reports the total number of cycles
# spent in the WRAM-to-WRAM copy operation. With MEM=MRAM, each tasklet instead
# reports the total number of cycles spent in MRAM processing: DMA read,
# WRAM-to-WRAM copy, and DMA write.

set -e

(

echo "prim-benchmarks WRAM microbenchmark (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for ndpu in 1 4 8 16 32 48 64 128 256; do
	for ntask in 1 2 4 8 16; do
		for bl in 4 5 6 7 8 9 10 11; do
			for op in streaming strided random; do
				if make -B NR_DPUS=$ndpu NR_TASKLETS=$ntask BL=$bl OP=$op; then
					bin/host_code -w 0 -e 50 || true
				fi
			done
		done
	done
done
echo "Completed at $(date)"
) | tee "log-$(hostname)-wram.txt"
rm -f "log-$(hostname)-wram.txt.xz"
xz -v -9 -M 800M "log-$(hostname)-wram.txt"

(

echo "prim-benchmarks WRAM microbenchmark (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for ndpu in 1 4 8 16 32 48 64 128 256; do
	for ntask in 1 2 4 8 16; do
		for bl in 4 5 6 7 8 9 10 11; do
			for op in streaming strided random; do
				if make -B NR_DPUS=$ndpu NR_TASKLETS=$ntask BL=$bl OP=$op MEM=MRAM; then
					bin/host_code -w 0 -e 50 || true
				fi
			done
		done
	done
done
echo "Completed at $(date)"
) | tee "log-$(hostname)-mram.txt"
rm -f "log-$(hostname)-mram.txt.xz"
xz -v -9 -M 800M "log-$(hostname)-mramd.txt"
