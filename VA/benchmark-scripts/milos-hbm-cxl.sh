#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=1 numa=1 perf_lib=0

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl

# 2^29 * 4B == 2 GiB per array → max 6 GiB total
# Runtime on milos: 2.5h
parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./va -i {input_size} -t {nr_threads} -A {ram_in} -B {ram_out} -C {cpu} -w 1 -e 5 \
	::: nr_threads 1 2 4 8 12 16 \
	:::   ram_in $(seq 0 17) \
	:::      cpu $(seq 0 7) $(seq 0 7) 0 4 \
	:::+ ram_out $(seq 0 17) \
	::: input_size $(( 2 ** 29 )) \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
