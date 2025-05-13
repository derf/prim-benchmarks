#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/ccmcc25

source /opt/upmem/upmem-2025.2.0-Linux-x86_64/upmem_env.sh

# Args: -m m -n n -o M_ -p N_
#
# Input: (M_ * m) × (N_ * n) matrix
# Output: (N_* n) × (M_ * m) matrix
# Step 1: transpose (M_ * m) × N_ matrix that consists of tiles of size n
#   CPU version: explicit
#   DPU version: implicit (M_ * m write operations of #DPUs * n elements to DPUs)
# Step 2: transpose m × n matrix; this happens N_ * M_ times.
#   DPU version: Each tasklet transposes a single m × n matrix / tile.
#   (16 × 8 tile takes up 1 KiB WRAM)
# Step 3: Transpose M_ × n matrix that consists of tiles of size m.
#
# Note for DPU version: if M_ > #DPUs, steps 1 through 3 are repeated.
# Number of repetitions == ceil(M_ / #DPUS)
# For Hetsim benchmarks, we set M_ == #DPUs to simplify the task graph (no repetitions that depend on the number of available DPUs).
# Just in case, there is also a configuration with M_ == 2048 independent of #DPUs
#
# input size: uint64(DPU)/double(CPU) * M_ * m * N_ * n
# output size: uint64(DPU)/double(CPU) * M_ * m * N_ * n -- on DPU only; CPU version operates in-place
# Upstream DPU version uses int64_t, -p 2048 -o 12288 -x 1 [implicit -m 16 -n 8]
# Upstream CPU version uses double,  -p 2556 -o 4096 -m 16 -n 8 and fails with -o 12288 (allocation error)
#
# -p         2048 -o 2048 -m 16 -n 8 -> matrix size: 4 GiB
# -p [64 .. 2304] -o 2048 -m 16 -n 8 -> matrix size: 128 MiB .. 4.5 GiB

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} dfatool_timing=0 aspectc=1 aspectc_timing=1
	bin/host_code -w 0 -e 40 -p ${p} -o 2048 -m 16 -n 8
}

export -f run_benchmark_nmc

(

echo "NMC single-node operation (1/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc p={nr_dpus} nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node operation (2/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc p={nr_dpus} nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	::: numa_rank any \
	::: nr_dpus 1536 2048 2304

echo "NMC single-node operation (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	run_benchmark_nmc p=2048 nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node operation (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	run_benchmark_nmc p=2048 nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	::: numa_rank any \
	::: nr_dpus 1536 2048 2304

) >> ${fn}.txt
