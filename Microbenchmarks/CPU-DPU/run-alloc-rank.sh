#!/bin/sh

set -e

echo "prim-benchmarks CPU-DPU alloc (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# runtime exclusive of host_code execution time: 25 seconds per inner loop
# *16  -> about 7 minutes per outer loop
# *163 -> about 18 hours total
for i in 1 4 8 16 32 48 64; do
	for j in $(seq 0 16); do
		echo $i/64 $j/16
		./make-size.sh $j
		n_nops=$((j * 256))
		if make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 DPU_BINARY=\'\"bin/dpu_size\"\'; then
			for l in $(seq 1 100); do
				bin/host_code -w 1 -e 0 -x 1 -i 65536 -N $n_nops || true
			done
		fi
	done
done

echo "Completed at $(date)"
