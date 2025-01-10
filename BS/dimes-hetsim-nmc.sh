#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-nmc

source /opt/upmem/upmem-2024.1.0-Linux-x86_64/upmem_env.sh

# upstream DPU version uses 2048576 * uint64 ≈ 16 MiB (DPU max: 64 MiB)
# upstream DPU version uses 2 queries
input_size_upstream=2048576
num_queries_upstream=2
# here: 32 MiB and 1048576 queries
input_size_dpu=$(perl -E 'say 2 ** 22')
num_queries_dpu=1048576

# Make sure that num_queries > input_size!

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1 INPUT_SIZE=${input_size} PROBLEM_SIZE=${num_queries}
	bin/bs_host -w 0 -e 100 2>&1
}

export -f run_benchmark_nmc

run_benchmark_baseline() {
	local "$@"
	set -e
	OMP_NUM_THREADS=${nr_threads} ./bs_omp ${input_size} ${num_queries} ${ram} ${cpu} ${ram_local} ${cpu_memcpy} 2>&1
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

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	num_queries=${num_queries_upstream} input_size=${input_size_upstream} \
	::: numa_rank -1 \
	::: nr_dpus 1536 2048 2304

echo "NMC single-node DPU-ref (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	num_queries=${num_queries_dpu} input_size=${input_size_dpu} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node DPU-ref (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	num_queries=${num_queries_dpu} input_size=${input_size_dpu} \
	::: numa_rank -1 \
	::: nr_dpus 1536 2048 2304

) >> ${fn}.txt

cd baselines/cpu

(

make -B numa=1 numa_memcpy=1

echo "CPU single-node upstream-ref with memcpy, copy node == input node (1/6)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	ram_local={ram_local} cpu_memcpy={cpu_memcpy} \
	num_queries=${num_queries_upstream} input_size=${input_size_upstream} \
	::: i $(seq 1 20) \
	:::         ram 0 1 \
	:::+ cpu_memcpy 0 1 \
	::: ram_local 0 1 \
	:::+      cpu 0 1 \
	::: nr_threads 1 2 4 8 12 16

echo "CPU single-node dpu-ref with memcpy, copy node == input node (2/6)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	ram_local={ram_local} cpu_memcpy={cpu_memcpy} \
	num_queries=${num_queries_dpu} input_size=${input_size_dpu} \
	::: i $(seq 1 20) \
	:::         ram 0 1 \
	:::+ cpu_memcpy 0 1 \
	::: ram_local 0 1 \
	:::+      cpu 0 1 \
	::: nr_threads 1 2 4 8 12 16

make -B numa=1

echo "CPU single-node upstream-ref (3/6)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	num_queries=${num_queries_upstream} input_size=${input_size_upstream} \
	::: i $(seq 1 20) \
	::: cpu 0 1 \
	::: ram 0 1 \
	::: nr_threads 1 2 4 8 12 16

echo "CPU single-node DPU-ref (4/6)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	num_queries=${num_queries_dpu} input_size=${input_size_dpu} \
	::: i $(seq 1 20) \
	::: cpu 0 1 \
	::: ram 0 1 \
	::: nr_threads 1 2 4 8 12 16

echo "CPU multi-node upstream-ref (5/6)" >&2

parallel -j1 --eta --joblog ${fn}.5.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	num_queries=${num_queries_upstream} input_size=${input_size_upstream} \
	::: i $(seq 1 20) \
	::: cpu -1 \
	::: ram 0 1 \
	::: nr_threads 24 32

echo "CPU multi-node DPU-ref (6/6)" >&2

parallel -j1 --eta --joblog ${fn}.6.joblog --resume --header : \
	run_benchmark_baseline i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	num_queries=${num_queries_dpu} input_size=${input_size_dpu} \
	::: i $(seq 1 20) \
	::: cpu -1 \
	::: ram 0 1 \
	::: nr_threads 24 32

) >> ${fn}.txt
