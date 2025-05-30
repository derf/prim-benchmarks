#!/bin/bash

mkdir -p log/$(hostname)

./make-size.sh 0

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_RANKS=${nr_ranks} NR_TASKLETS=1 BL=10 TRANSFER=PUSH NUMA=1
	bin/host_code -a $numa_in -b $numa_out -c $numa_cpu -w 0 -e 20 -x 0 -N 0 -I $(size -A bin/dpu_code | awk '($1 == ".text") {print $2/8}') -i ${input_size}
	return $?
}

export -f run_benchmark_nmc

# The benchmark allocates 3 * 64 * nr_ranks * 8B * input_size (one array for input, one array for output).
# With 1048576 elements (8 MiB per DPU), this gives a maximum allocation of 60 GiB, which will fit comfortably into system memory (128 GiB).
# With 2097152 elements (16 MiB per DPU), we may encounter OoM conditions, since the UPMEM SDK also allocates some memory.

for sdk in 2025.1.0-orig 2025.1.0-notransform; do
	source /opt/upmem/transformation-benchmarks/${sdk}/upmem_env.sh
	fn=log/$(hostname)/upvec-${sdk}

	parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
		run_benchmark_nmc nr_ranks={nr_ranks} numa_rank={numa_rank} numa_in={numa_in} numa_out={numa_out} numa_cpu={numa_cpu} input_size={input_size} \
		::: numa_rank any \
		::: numa_in 1 \
		::: numa_out 1 \
		::: numa_cpu 1 \
		::: nr_ranks $(seq 1 40) \
		::: input_size 1048576 \
	>> ${fn}.txt
done
