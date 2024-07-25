#!/bin/sh

cd baselines/cpu

mkdir -p log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-hbm

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

(

make -B NUMA=1 NUMA_MEMCPY=1

echo "CPU single-node operation with setup cost, memcpy node == input node, cpu node == output node (1/3)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./trns -w 0 -r 5 -p {p} -o 2048 -m 16 -n 8 -t {nr_threads} -a {ram_in} -c {cpu} -C {ram_local} -M {cpu_memcpy} \
	::: p 64 128 256 512 768 1024 1536 2048 2304 \
	::: nr_threads 1 2 4 8 12 16 \
	:::      ram_in $(seq 0 15) \
	:::+ cpu_memcpy $(seq 0 7) $(seq 0 7) \
	:::   ram_local $(seq 0 15) \
	:::+        cpu $(seq 0 7) $(seq 0 7) \
	::: input_size 167772160

make -B NUMA=1

echo "CPU single-node operation (2/3)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./trns -w 0 -r 5 -p {p} -o 2048 -m 16 -n 8 -t {nr_threads} -a {ram} -c {cpu} \
	::: p 64 128 256 512 768 1024 1536 2048 2304 \
	::: ram $(seq 0 15) \
	::: cpu $(seq 0 7) \
	::: nr_threads 1 2 4 8 12 16

echo "CPU multi-node operation (3/3)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	./trns -w 0 -r 40 -p {p} -o 2048 -m 16 -n 8 -t {nr_threads} -a {ram} -c {cpu} \
	::: p 64 128 256 512 768 1024 1536 2048 2304 \
	::: ram $(seq 0 15) \
	::: cpu -1 \
	::: nr_threads 32 48 64 96 128

) >> ${fn}.txt
