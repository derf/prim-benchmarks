#!/bin/bash

mkdir -p log/$(hostname)

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
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets}  \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/host_code -w 0 -e 5 -p ${cols} -o ${rows} -m ${tile_rows} -n ${tile_cols}
}

export -f run_benchmark_nmc

fn=log/$(hostname)/ccmcc25-sdk${sdk}-sim

source ~/lib/local/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh simulator

echo "prim-benchmarks  TRNS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	::: nr_dpus 1 2 4 8 16 32 48 64 \
	::: rows 64 128 256 512 \
	::: cols 64 128 256 512 \
	::: tile_rows 16 \
	::: tile_cols 8 \
>> ${fn}.txt
