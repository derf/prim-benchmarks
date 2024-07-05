#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# Upstream DPU version uses 256 bins and 1536 * 1024 * 64 uint32 elements == 384 MiB total (-x 2 with implicit -z 64)
input_size_upstream=$((1536 * 1024 * 64))

# Here: 2 GiB
input_size_dpu=$((2**29))

run_benchmark_nmc() {
	local "$@"
	sudo limit_ranks_to_numa_node ${numa_rank}
	if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
		bin/host_code -w 0 -e 100 -b 256 -x 1 -i ${input_size}
	fi
	return $?
}

export -f run_benchmark_nmc

(

echo "NMC single-node upstream-ref (1/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	input_size=${input_size_upstream} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node upstream-ref (2/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	input_size=${input_size_upstream} \
	::: numa_rank -1 \
	::: nr_dpus 1536 2048 2304

echo "NMC single-node DPU-ref (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	input_size=${input_size_dpu} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node DPU-ref (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	input_size=${input_size_dpu} \
	::: numa_rank -1 \
	::: nr_dpus 1536 2048 2304

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt

cd baselines/cpu
make -B NUMA=1

(

echo "CPU single-node upstream-ref (1/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./hist -i ${input_size_upstream} -A {ram} -B {ram} -C {cpu} -t {nr_threads} -w 0 -e 20 -x 1 \
	::: cpu 0 1 \
	::: ram 0 1 \
	::: nr_threads 1 2 4 8 12 16 32

echo "CPU single-node DPU-ref (2/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./hist -i ${input_size_dpu} -A {ram} -B {ram} -C {cpu} -t {nr_threads} -w 0 -e 20 -x 1 \
	::: i $(seq 1 20) \
	::: cpu 0 1 \
	::: ram 0 1 \
	::: nr_threads 1 2 4 8 12 16 32

echo "CPU multi-node upstream-ref (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	./hist -i ${input_size_upstream} -A {ram} -B {ram} -C {cpu} -t {nr_threads} -w 0 -e 20 -x 1 \
	::: i $(seq 1 20) \
	::: cpu -1 \
	::: ram 0 1 \
	::: nr_threads 48 64

echo "CPU multi-node DPU-ref (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	./hist -i ${input_size_dpu} -A {ram} -B {ram} -C {cpu} -t {nr_threads} -w 0 -e 20 -x 1 \
	::: i $(seq 1 20) \
	::: cpu -1 \
	::: ram 0 1 \
	::: nr_threads 48 64

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
