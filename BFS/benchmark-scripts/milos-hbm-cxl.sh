#!/bin/bash

cd baselines/cpu

mkdir -p log/$(hostname)

make -B dfatool_timing=1 numa=1 perf_lib=0

fn=log/$(hostname)/milos-hbm-cxl

# Runtime on milos: <1h
parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./bfs -f ../../data/loc-gowalla_edges.txt -t {nr_threads} -A {numa_data_in} -C {numa_compute} -w 1 -e 10 \
	::: nr_threads 1 2 4 8 12 16 \
	::: numa_data_in $(seq 0 17) \
	::: numa_compute $(seq 0 7) \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
