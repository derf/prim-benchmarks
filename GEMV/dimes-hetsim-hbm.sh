#!/bin/bash

cd baselines/cpu

mkdir -p log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-hbm

# upstream DPU version uses -m 163840 -n 4096
# → (163840 * 4096 + 4096) * uint32 ≈ 2.5 GiB

# upstream baseline uses 20480 rows and 8192 cols, allocating 20480 * double + 8192 * double + 20480 * double + 20480 * 8192 * double
# → ≈ 1.25 GiB

run_benchmark() {
	local "$@"
	OMP_NUM_THREADS=$nr_threads ./gemv $ram_in $ram_out $cpu $ram_local $cpu_memcpy
	return $?
}

export -f run_benchmark

(

make -B NUMA=1 NUMA_MEMCPY=1

echo "CPU single-node operation with setup cost, memcpy node == input node, cpu node == output node (1/3)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark nr_threads={nr_threads} ram_in={ram_in} ram_out={ram_out} ram_local={ram_local} cpu={cpu} cpu_memcpy={cpu_memcpy} \
	::: nr_threads 1 2 4 8 12 16 \
	:::      ram_in $(seq 0 15) \
	:::+ cpu_memcpy $(seq 0 7) $(seq 0 7) \
	:::   ram_local $(seq 0 15) \
	:::+        cpu $(seq 0 7) $(seq 0 7) \
	:::+    ram_out $(seq 0 15)

make -B NUMA=1

echo "single-node execution, cpu/out on same node (2/3)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark nr_threads={nr_threads} ram_in={ram_in} ram_out={ram_out} cpu={cpu} \
	::: nr_threads 1 2 4 8 12 16 \
	:::   ram_in $(seq 0 15) \
	:::      cpu $(seq 0 7) $(seq 0 7) \
	:::+ ram_out $(seq 0 15)

echo "multi-node execution (3/3)" >&2
parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	run_benchmark nr_threads={nr_threads} ram_in={ram_in} ram_out={ram_out} cpu={cpu} \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram_in $(seq 0 15) \
	::: ram_out $(seq 0 15)

) >> ${fn}.txt
