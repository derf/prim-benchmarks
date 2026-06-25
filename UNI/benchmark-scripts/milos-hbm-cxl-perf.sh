#!/bin/bash

cd baselines/cpu
make -B dfatool_timing=0 numa=1 perf_lib=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl-perf

# 2^28 * 8B == 2 GiB per array → max 4 GiB total
parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./uni -i {input_size} -t {nr_threads} -A {ram_in} -B {ram_out} -C {cpu} -w 1 -e 20 \
	::: nr_threads 1 2 4 8 12 16 \
	:::   ram_in 4 \
	:::      cpu 4 \
	:::+ ram_out 4 \
	::: input_size $(( 2 ** 28 )) \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
