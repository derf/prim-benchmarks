#!/bin/sh

set -e

echo "prim-benchmarks CPU-DPU alloc (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

./make-size.sh 0

for i in $(seq 1 20); do
	for k in BROADCAST; do
		# BROADCAST sends the same data to all DPUs, so data size must not exceed the amount of MRAM available on a single DPU (i.e., 64 MB)
		for l in 4194304 6291456; do
			make -B NR_RANKS=$i NR_TASKLETS=1 BL=10 TRANSFER=$k NUMA=1
			for numa_rank in 0 1; do
				sudo limit_ranks_to_numa_node $numa_rank
				for numa_in in 0 1; do
					for numa_out in 0 1; do
						for numa_cpu in 0 1; do
							bin/host_code -a $numa_in -b $numa_out -c $numa_cpu -w 0 -e 100 -x 1 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}')  -i $l
						done
					done
				done
			done
		done
	done

	# utilize 32MiB / 50% of per-DPU MRAM capacity -- otherwise DRAM capacity per NUMA node is insufficient
	for numa_rank in 0 1; do
		sudo limit_ranks_to_numa_node $numa_rank
		for numa_in in 0 1; do
			for numa_out in 0 1; do
				for numa_cpu in 0 1; do
					make -B NR_RANKS=$i NR_TASKLETS=1 BL=10 TRANSFER=PUSH NUMA=1
					bin/host_code -a $numa_in -b $numa_out -c $numa_cpu -w 0 -e 100 -x 0 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}') -i 1
					bin/host_code -a $numa_in -b $numa_out -c $numa_cpu -w 0 -e 100 -x 0 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}') -i 4194304
					make -B NR_RANKS=$i NR_TASKLETS=1 BL=10 TRANSFER=SERIAL NUMA=1
					bin/host_code -a $numa_in -b $numa_out -c $numa_cpu -w 0 -e 100 -x 0 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}') -i 1
				done
			done
		done
	done
done

sudo limit_ranks_to_numa_node any

echo "Completed at $(date)"
