#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=0 numa=1 perf_lib=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl-perf

# 2048 * 2048 * 16 * 8 * 8B == 4 GiB
parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./trns -p 2048 -o 2048 -m 16 -n 8 -t {nr_threads} -A {ram_in} -C {cpu} -w 1 -e 20 \
	::: nr_threads 1 2 4 8 12 16 \
	:::   ram_in 4 \
	:::      cpu 4 \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
