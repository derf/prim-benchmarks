#!/bin/bash

cd baselines/cpu

mkdir -p log/$(hostname)

make -B dfatool_timing=0 numa=1 perf_lib=1

fn=log/$(hostname)/milos-hbm-cxl-perf

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./bfs -f ../../data/loc-gowalla_edges.txt -t {nr_threads} -A {numa_data_in} -C {numa_compute} -w 1 -e 20 \
	::: nr_threads 1 2 4 8 12 16 \
	::: numa_data_in 4 \
	::: numa_compute 4 \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
