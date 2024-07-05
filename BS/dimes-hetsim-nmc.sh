#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# upstream DPU version uses 2048576 * uint64 ≈ 16 MiB (DPU max: 64 MiB)
# upstream DPU version uses 2 queries
input_size_upstream=2048576
num_queries_upstream=2
# here: 32 MiB and 1048576 queries
input_size_dpu=$(perl -E 'say 2 ** 22')
num_queries_dpu=1048576

run_benchmark_nmc() {
	local "$@"
	sudo limit_ranks_to_numa_node ${numa_rank}
	if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1 INPUT_SIZE=${input_size} PROBLEM_SIZE=${num_queries}; then
		bin/bs_host -w 0 -e 100
	fi
	return $?
}

export -f run_benchmark_nmc

run_benchmark_baseline() {
	local "$@"
	OMP_NUM_THREADS=${nr_threads} ./bs_omp ${input_size} ${num_queries} ${ram} ${cpu}
	return $?
}

export -f run_benchmark_baseline

(

echo "NMC single-node upstream-ref (1/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	num_queries=${num_queries_upstream} input_size=${input_size_upstream} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node upstream-ref (2/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	num_queries=${num_queries_upstream} input_size=${input_size_upstream} \
	::: numa_rank -1 \
	::: nr_dpus 1536 2048 2304

echo "NMC single-node DPU-ref (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	num_queries=${num_queries_dpu} input_size=${input_size_dpu} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node DPU-ref (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	num_queries=${num_queries_dpu} input_size=${input_size_dpu} \
	::: numa_rank -1 \
	::: nr_dpus 1536 2048 2304

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt

cd baselines/cpu
make -B NUMA=1

(

echo "CPU single-node upstream-ref (1/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	num_queries=${num_queries_upstream} input_size=${input_size_upstream} \
	::: i $(seq 1 20) \
	::: cpu 0 1 \
	::: ram 0 1 \
	::: nr_threads 1 2 4 8 12 16 32

echo "CPU single-node DPU-ref (2/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	num_queries=${num_queries_dpu} input_size=${input_size_dpu} \
	::: i $(seq 1 20) \
	::: cpu 0 1 \
	::: ram 0 1 \
	::: nr_threads 1 2 4 8 12 16 32

echo "CPU multi-node upstream-ref (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	num_queries=${num_queries_upstream} input_size=${input_size_upstream} \
	::: i $(seq 1 20) \
	::: cpu -1 \
	::: ram 0 1 \
	::: nr_threads 48 64

echo "CPU multi-node DPU-ref (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	num_queries=${num_queries_dpu} input_size=${input_size_dpu} \
	::: i $(seq 1 20) \
	::: cpu -1 \
	::: ram 0 1 \
	::: nr_threads 48 64

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
