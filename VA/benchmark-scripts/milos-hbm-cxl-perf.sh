#!/bin/sh

cd baselines/cpu
make -B benchmark=0 numa=1 perf_lib=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl-perf

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -t {nr_threads} -w 1 -e 20 \
	::: nr_threads 1 2 4 8 12 16 \
	:::   ram_in 0 \
	:::      cpu 0 \
	:::+ ram_out 0 \
	::: input_size 167772160 \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
