#!/bin/sh

set -e

echo "prim-benchmarks CPU-DPU alloc (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for i in $(seq 1 20); do
	for rank_node in 0 1; do
		sudo limit_ranks_to_numa_node $rank_node
		for j in $(seq 0 16); do
			echo $i/20 $j/16
			./make-size.sh $j
			n_nops=$((j * 256))
			if make -B NR_RANKS=$i NR_TASKLETS=1 BL=10 DPU_BINARY=\'\"bin/dpu_size\"\' NUMA=1; then
				for l in $(seq 1 100); do
					bin/host_code -c 0 -w 1 -e 0 -x 1 -i 65536 -N $n_nops -I $(size -A bin/dpu_size | awk '($1 == ".text") {print $2/8}') || true
					bin/host_code -c 1 -w 1 -e 0 -x 1 -i 65536 -N $n_nops -I $(size -A bin/dpu_size | awk '($1 == ".text") {print $2/8}') || true
				done
			fi
		done
	done

done

sudo limit_ranks_to_numa_node any

echo "Completed at $(date)"
