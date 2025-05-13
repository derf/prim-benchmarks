#!/bin/bash

cd baselines/cpu
make -B numa=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl

# * uint64 == 128 MiB
num_queries_hbm=16777216

run_benchmark() {
	local "$@"
	OMP_NUM_THREADS=${nr_threads} ./bs_omp ${input_size} ${num_queries} $ram $cpu 2>&1
	return $?
}

export -f run_benchmark

(

echo "single-node execution, HBM ref (1/2)" >&2

# 4 GiB
parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	input_size=$(perl -E 'say 2 ** 29') num_queries=${num_queries_hbm} \
	::: i $(seq 1 5) \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 16)

echo "multi-node execution, HBM ref (2/2)" >&2

# 8 GiB
parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	input_size=$(perl -E 'say 2 ** 30') num_queries=${num_queries_hbm} \
	::: i $(seq 1 40) \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 16)

) >> ${fn}.txt
