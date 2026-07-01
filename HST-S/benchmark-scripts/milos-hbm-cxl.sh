#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=1 numa=1 perf_lib=0

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl

# input_size % p.n_threads == 0  →  nr_threads must be power of two

# Runtime on milos: 13h
parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./hist -w 1 -e 5 -i $(( 2 ** 29 )) -t {nr_threads} -A {numa_data_in} -B {numa_data_out} -C {numa_compute} \
		::: nr_threads 1 2 4 8 16 \
		::: numa_data_in $(seq 0 17) \
		::: numa_compute $(seq 0 7) $(seq 0 7) 0 4 \
		:::+ numa_data_out $(seq 0 17) \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
