#!/bin/bash

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d).t

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

# 16 MiB per DPU

(

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc nr_ranks={nr_ranks} numa_rank={numa_rank} numa_in={numa_in} numa_out={numa_out} numa_cpu={numa_cpu} input_size={input_size} \
	::: i $(seq 1 5) \
	::: numa_rank 0 1 \
	::: numa_in 0 1 \
	::: numa_out 0 1 \
	::: numa_cpu 0 1 \
	::: nr_ranks $(seq 1 20) \
	::: input_size 1 2097152

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc nr_ranks={nr_ranks} numa_rank={numa_rank} numa_in={numa_in} numa_out={numa_out} numa_cpu={numa_cpu} input_size={input_size} \
	::: i $(seq 1 5) \
	::: numa_rank any \
	::: numa_in 0 1 \
	::: numa_out 0 1 \
	::: numa_cpu 0 1 \
	::: nr_ranks $(seq 21 40) \
	::: input_size 1 2097152

) >> ${fn}.txt
