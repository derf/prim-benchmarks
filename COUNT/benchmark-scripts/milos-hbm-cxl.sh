#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=1 numa=1 perf_lib=0

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl

# 2^29 * 8B == 4 GiB

run_benchmark() {
	local "$@"
	set -e
	./count -t ${nr_threads} -i $(( 2 ** 29 )) -A ${numa_data_in} -C ${numa_compute} -w 1 -e 10
}

export -f run_benchmark

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	run_benchmark nr_threads={nr_threads} numa_data_in={numa_data_in} numa_compute={numa_compute} \
		::: nr_threads 1 2 4 8 12 16 \
		::: numa_compute $(seq 0 7) \
		::: numa_data_in $(seq 0 17) \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
