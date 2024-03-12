#!/bin/sh

set -e

echo "prim-benchmarks CPU-DPU transfer (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

./make-size.sh 0

completion=0
for i in 1 4 8 $(seq 16 16 2543 | shuf); do
	nrep=$(perl -E 'my $r = int(7700/$ARGV[0]); say $r > 60 ? 60 : $r' "$i")
	for k in BROADCAST; do
		completion=$((completion+1))
		echo "Running ${completion}/161 at $(date)"
		# BROADCAST sends the same data to all DPUs, so data size must not exceed the amount of MRAM available on a single DPU (i.e., 64 MB)
		for l in 4194304 6291456 8388608; do
			if make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 TRANSFER=$k; then
				bin/host_code -w 0 -e $nrep -x 1 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}')  -i $l
			fi
		done
	done
	for k in SERIAL PUSH; do
		echo "Running at $(date)"
		if make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 TRANSFER=$k; then
			bin/host_code -w 0 -e 60 -x 1 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}') -i 8388608
			# utilize 50% to 100% of per-DPU MRAM capacity
			bin/host_code -w 0 -e $nrep -x 1 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}') -i $(( 4194304 * i ))
			#bin/host_code -w 0 -e $nrep -x 1 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}') -i $(( 6291456 * i ))
			#bin/host_code -w 0 -e $nrep -x 1 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}') -i $(( 8388608 * i ))
		fi
	done
done

echo "Completed at $(date)"
