#!/bin/sh

set -e

echo "prim-benchmarks CPU-DPU transfer (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

./make-size.sh 0

for i in 1 4 8 16 32 48 64; do
	for k in SERIAL PUSH BROADCAST; do
		# BROADCAST sends the same data to all DPUs, so data size must not exceed the amount of MRAM available on a single DPU (i.e., 64 MB)
		# 8 B ... 64 MB
		for l in 1 16 256 4096 65536 262144 1048576 4194304 6291456 8388608; do
			make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 TRANSFER=$k
			bin/host_code -w 0 -e 100 -x 1 -i $l
		done
	done
	# maximum amount of data
	for k in SERIAL PUSH; do
		make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 TRANSFER=$k
		bin/host_code -w 0 -e 100 -x 1 -i $(( 4194304 * i ))
		bin/host_code -w 0 -e 100 -x 1 -i $(( 6291456 * i ))
		bin/host_code -w 0 -e 100 -x 1 -i $(( 8388608 * i ))
	done
done
