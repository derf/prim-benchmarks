#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=1 numa=1 perf_lib=0

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl

# 2^28 elements * 8 B == 2 GiB
input_size=$((2**28))

# 2^22 queries * 8 B == 32 MiB
num_queries=$((2**22))

# Runtime on milos: 8.5h
parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./bs_omp -t {nr_threads} -w 1 -e 5 -A {ram} -C {cpu} -i ${input_size} -q ${num_queries} \
		::: nr_threads 1 2 4 8 12 16 \
		::: cpu $(seq 0 7) \
		::: ram $(seq 0 17) \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
