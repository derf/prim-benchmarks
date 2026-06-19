#!/bin/bash

cd baselines/cpu

mkdir -p log/$(hostname)

run_benchmark() {
	local "$@"
	set -e
	OMP_NUM_THREADS=${nr_threads} ./bfs -f ../../data/loc-gowalla_edges.txt -A ${numa_data_in} -C ${numa_compute} -w 1 -e 20
}

export -f run_benchmark

make -B dfatool_timing=0 numa=1 perf_lib=1

fn=log/$(hostname)/milos-hbm-cxl-perf

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	run_benchmark nr_threads={nr_threads} numa_data_in={numa_data_in} numa_compute={numa_compute} \
	::: nr_threads 1 2 4 8 12 16 \
	::: numa_data_in 4 \
	::: numa_compute 4 \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
