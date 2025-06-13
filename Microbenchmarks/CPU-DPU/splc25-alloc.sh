#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	./make-size.sh ${size}
	n_nops=$((size * 256))
	if make -B NR_RANKS=${nr_ranks} NR_TASKLETS=1 BL=10 DPU_BINARY=\'\"bin/dpu_size\"\' NUMA=1; then
		for l in $(seq 1 20); do
			bin/host_code -c ${numa_cpu} -w 1 -e 0 -x 1 -i 65536 -N $n_nops -I $(size -A bin/dpu_size | awk '($1 == ".text") {print $2/8}')
		done
	fi
	return $?
}

export -f run_benchmark_nmc

for sdk in 2023.2.0 2024.1.0 2024.2.0 2025.1.0; do

	source /opt/upmem/upmem-${sdk}-Linux-x86_64/upmem_env.sh
	fn=log/$(hostname)/splc25-alloc-${sdk}

	parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
		run_benchmark_nmc nr_ranks={nr_ranks} numa_rank={numa_rank} numa_cpu={numa_cpu} size={size} \
		::: i $(seq 1 5) \
		::: numa_rank -1 \
		::: numa_cpu 0 1 \
		::: nr_ranks $(seq 1 40) \
		::: size $(seq 0 15) \
	>> ${fn}.txt

done
