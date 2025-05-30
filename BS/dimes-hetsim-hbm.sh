#!/bin/bash

cd baselines/cpu
make -B numa=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-hbm

# upstream DPU version uses 2048576 * uint64 ≈ 16 MiB (DPU max: 64 MiB)
# upstream DPU version uses 2 queries
# * uint64 == 32 MiB
input_size_dpu=$(perl -E 'say 2 ** 22')
# * uint64 == 8 MiB
num_queries_dpu=1048576

# * uint64 == 4 GiB
input_size_hbm=$(perl -E 'say 2 ** 29')
# * uint64 == 128 MiB
num_queries_hbm=16777216

run_benchmark() {
	local "$@"
	OMP_NUM_THREADS=${nr_threads} ./bs_omp ${input_size} ${num_queries} $ram $cpu 2>&1
	return $?
}

export -f run_benchmark

(

echo "single-node execution, DPU ref (1/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	input_size=${input_size_dpu} num_queries=${num_queries_dpu} \
	::: i $(seq 1 5) \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15)

echo "single-node execution, HBM ref (2/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	input_size=${input_size_hbm} num_queries=${num_queries_hbm} \
	::: i $(seq 1 5) \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15)

echo "multi-node execution, DPU ref (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	run_benchmark i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	input_size=${input_size_dpu} num_queries=${num_queries_dpu} \
	::: i $(seq 1 40) \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 15)

echo "multi-node execution, HBM ref (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	run_benchmark i={i} nr_threads={nr_threads} ram={ram} cpu={cpu} \
	input_size=${input_size_hbm} num_queries=${num_queries_hbm} \
	::: i $(seq 1 40) \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 15)

) >> ${fn}.txt
