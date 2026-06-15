#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=0 numa=0 perf=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl

run_benchmark() {
	local "$@"
	set -e
	# warmup
	OMP_NUM_THREADS=${nr_threads} ./bs_omp ${input_size} ${num_queries} $ram $cpu > /dev/null
	for i in $(seq 1 5); do
		OMP_NUM_THREADS=${nr_threads} ./bs_omp ${input_size} ${num_queries} $ram $cpu
	done
}

export -f run_benchmark

# 2^28 elements * 8 B == 2 GiB
# 2^24 queries * 8 B == 128 MiB

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	run_benchmark nr_threads={nr_threads} ram={ram} cpu={cpu} \
		::: input_size $((2**28)) \
		::: num_queeries $((2**24)) \
		::: nr_threads 1 2 4 8 12 16 \
		::: cpu $(seq 0 7) \
		::: ram $(seq 0 17) \
> ${fn}.txt
