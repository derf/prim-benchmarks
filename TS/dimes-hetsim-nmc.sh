#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# Upstream DPU version uses ts_size = 33554432 elements and query_length = 256 with int32_t data
# Upstream CPU version uses inputs/randomlist33M.txt with 33618177 elements and query_length = 256 with double
# This benchmark uses int32 and 2²⁵ == 33554432 elements for both.

run_benchmark_nmc() {
	local "$@"
	sudo limit_ranks_to_numa_node ${numa_rank}
	if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=8 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
		bin/ts_host -w 0 -e 100 -n ${input_size}
	fi
	return $?
}

export -f run_benchmark_nmc

run_benchmark() {
	local "$@"
	OMP_NUM_THREADS=${nr_threads} ./streamp_openmp inputs/randomlistDPU.txt 256 ${ram} ${cpu}
}

export -f run_benchmark

(

echo "NMC single-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=8 input_size={input_size} numa_rank={numa_rank} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024 \
	::: input_size 33554432

echo "NMC multi-node operation (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=8 input_size={input_size} numa_rank={numa_rank} \
	::: numa_rank any \
	::: nr_dpus 1536 2048 2304 \
	::: input_size 33554432

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt

cd baselines/cpu
make -B NUMA=1 DTYPE=int32_t
make inputs/randomlistDPU.txt

(

echo "CPU single-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --header : \
	run_benchmark nr_threads={nr_threads} ram={ram} cpu={cpu} \
	::: i $(seq 0 20) \
	::: ram 0 1 \
	::: cpu 0 1 \
	::: nr_threads 1 2 4 8 12 16

echo "CPU multi-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --header : \
	run_benchmark nr_threads={nr_threads} ram={ram} cpu={cpu} \
	::: i $(seq 0 20) \
	::: ram 0 1 \
	::: cpu -1 \
	::: nr_threads 24 32

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
