#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=0 numa=1 perf_lib=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl-perf

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./gemv -m 20480 -n 16384 -t {nr_threads} -A {numa_data_in} -B {numa_data_out} -C {numa_compute} -w 1 -e 20
		::: nr_threads 1 2 4 8 12 16 \
		::: numa_compute 4 \
		::: numa_data_in 4 \
		::: numa_data_out 4 \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
