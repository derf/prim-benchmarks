#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=0 numa=1 perf_lib=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl-perf

# 2^28 elements * 8 B == 2 GiB
input_size=$((2**28))

# 2^22 queries * 8 B == 32 MiB
num_queries=$((2**22))

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./bs_omp -t {nr_threads} -w 1 -e 20 -A {ram} -C {cpu} -i ${input_size} -q ${num_queries} \
		::: nr_threads 1 2 4 8 12 16 \
		::: cpu 4 \
		::: ram 4 \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
