#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=1 numa=1 perf_lib=0

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl

# runtime on milos: 2h
parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./gemv -m 20480 -n 16384 -t {nr_threads} -A {numa_data_in} -B {numa_data_out} -C {numa_compute} -w 1 -e 10
		::: nr_threads 1 2 4 8 12 16 \
		::: numa_data_in $(seq 0 17) \
		::: numa_compute $(seq 0 7) $(seq 0 7) 0 4 \
		:::+ numa_data_out $(seq 0 17) \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
