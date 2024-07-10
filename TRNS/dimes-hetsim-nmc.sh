#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# DPU version: -m m -n n -o M_ -p N_
# input size: sizeof(T) * M_ * m * N_ * n
# Upstream DPU version uses int64_t, -p 2048 -o 12288 -x 1 [implicit -m 16 -n 8]
# Upstream CPU version uses double,  -p 2556 -o 4096 -m 16 -n 8 and fails with -o 12288 (allocation error)
#
# Here: -p 2048 -o 2048 -m 16 -n 8 for both

# upstream uses 167772160 * 2 * int32 == 2.5 GiB input and 1.25 GiB output for DPU version

run_benchmark_nmc() {
	local "$@"
	sudo limit_ranks_to_numa_node ${numa_rank}
	if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
		bin/host_code -w 0 -e 100 -p 2048 -o 2048 -m 16 -n 8 -x 1
	fi
	return $?
}

export -f run_benchmark_nmc

(

echo "NMC single-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 input_size={input_size} numa_rank={numa_rank} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node operation (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 input_size={input_size} numa_rank={numa_rank} \
	::: numa_rank any \
	::: nr_dpus 1536 2048 2304

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt

cd baselines/cpu
make -B NUMA=1

(

echo "CPU single-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --header : \
	./trns -w 0 -r 40 -p 2048 -o 2048 -m 16 -n 8 -t {nr_threads} \
	::: ram 0 1 \
	::: cpu 0 1 \
	::: nr_threads 1 2 4 8 12 16 32

echo "CPU multi-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --header : \
	./trns -w 0 -r 40 -p 2048 -o 2048 -m 16 -n 8 -t {nr_threads} \
	::: ram 0 1 \
	::: cpu -1 \
	::: nr_threads 48 64

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
