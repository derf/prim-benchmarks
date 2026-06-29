#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=0 numa=1 perf_lib=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl-perf

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./mlp_openmp -w 1 -e 20 -t {nr_threads} -A {numa_data_in} -C {numa_compute} \
		::: nr_threads 1 2 4 8 12 16 \
		::: numa_compute 4 \
		::: numa_data_in 4 \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
