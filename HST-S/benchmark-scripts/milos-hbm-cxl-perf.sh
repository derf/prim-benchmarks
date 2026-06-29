#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=0 numa=1 perf_lib=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl-perf

# input_size % p.n_threads == 0  →  nr_threads must be power of two

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./hist -w 1 -e 20 -i $(( 2 ** 29 )) -t {nr_threads} -A {numa_data_in} -B {numa_data_out} -C {numa_compute} \
		::: nr_threads 1 2 4 8 16 \
		::: numa_compute 4 \
		::: numa_data_in 4 \
		::: numa_data_out 4 \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
