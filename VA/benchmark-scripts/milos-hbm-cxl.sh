#!/bin/sh

cd baselines/cpu
make -B benchmark=1 numa=1 perf_lib=0

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -t {nr_threads} -w 1 -e 5 \
	::: nr_threads 1 2 4 8 12 16 \
	:::   ram_in $(seq 0 17) \
	:::      cpu $(seq 0 7) $(seq 0 7) 0 4 \
	:::+ ram_out $(seq 0 17) \
	::: input_size 167772160 \
> ${fn}.txt

xz -f -v -9 ${fn}.txt
