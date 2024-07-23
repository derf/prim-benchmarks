#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-nmc

# upstream DPU version uses -m 163840 -n 4096
# → (163840 * 4096 + 4096) * uint32 ≈ 2.5 GiB

# upstream baseline uses 20480 rows and 8192 cols, allocating 20480 * double + 8192 * double + 20480 * double + 20480 * 8192 * double
# → ≈ 1.25 GiB

# Note: Upstream uses int32_t in DPU version and double in baseline.
# Here, we use (163840 * 4096 + 4096 ) * int32_t for both.

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1
	bin/gemv_host -w 0 -e 100 -m 163840 -n 4096
}

export -f run_benchmark_nmc

run_benchmark_baseline() {
	local "$@"
	set -e
	OMP_NUM_THREADS=$nr_threads ./gemv $ram_in $ram_out $cpu $ram_local $cpu_memcpy
}

export -f run_benchmark_baseline

(

echo "NMC single-node (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	::: numa_rank -1 \
	::: nr_dpus 1536 2048 2304

) >> ${fn}.txt

cd baselines/cpu

(

make -B NUMA=1 TYPE=int32_t NUMA_MEMCPY=1

echo "CPU single-node operation with setup cost, memcpy/in and cpu/out on same node (1/3)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_baseline nr_threads={nr_threads} ram_in={ram_in} ram_out={ram_out} cpu={cpu} ram_local={ram_local} cpu_memcpy={cpu_memcpy} \
	:::      ram_in 0 1 \
	:::+ cpu_memcpy 0 1 \
	:::        cpu 0 1 \
	:::+   ram_out 0 1 \
	:::+ ram_local 0 1 \
	::: nr_threads 1 2 4 8 12 16

make -B NUMA=1 TYPE=int32_t

echo "CPU single-node (2/3)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_baseline nr_threads={nr_threads} ram_in={ram_in} ram_out={ram_out} cpu={cpu} \
	::: cpu 0 1 \
	::: ram_in 0 1 \
	::: ram_out 0 1 \
	::: nr_threads 1 2 4 8 12 16

echo "CPU multi-node (3/3)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	run_benchmark_baseline nr_threads={nr_threads} ram_in={ram_in} ram_out={ram_out} cpu={cpu} \
	::: cpu -1 \
	::: ram_in 0 1 \
	::: ram_out 0 1 \
	::: nr_threads 24 32

) >> ${fn}.txt
