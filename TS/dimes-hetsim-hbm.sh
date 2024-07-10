#!/bin/bash

cd baselines/cpu
make -B NUMA=1
make inputs/randomlist10M.txt

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# Upstream DPU version uses ts_size = 33554432 elements and query_length = 256 with int32_t data
# Upstream CPU version uses inputs/randomlist33M.txt with 33618177 elements and query_length = 256 with double
# However, this does not work with ~64 or more threads due to an internal tmp array allocation failure in 'profile_tmp new DTYPE[ProfileLength * numThreads]' → use 10M elements instead.

run_benchmark() {
	local "$@"
	OMP_NUM_THREADS=${nr_threads} ./streamp_openmp inputs/randomlist10M.txt 256 ${ram} ${cpu}
}

export -f run_benchmark

(

echo "single-node execution (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --header : \
	run_benchmark nr_threads={nr_threads} ram={ram} cpu={cpu} \
	::: i $(seq 0 5) \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15)

echo "multi-node execution (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --header : \
	run_benchmark nr_threads={nr_threads} ram={ram} cpu={cpu} \
	::: i $(seq 0 20) \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 15)

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
