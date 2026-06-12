#!/bin/sh

cd baselines/cpu
make -B numa=1 perf=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/milos-hbm-cxl-perf

(

for i in $(seq 0 7); do
	echo "single-node execution ($i/8)" >&2
	parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
		./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -t {nr_threads} -w 1 -e 5 \
		::: nr_threads 1 2 4 8 12 16 \
		:::   ram_in $(seq 0 17) \
		:::      cpu $(seq 0 7) $(seq 0 7) 0 4 \
		:::+ ram_out $(seq 0 17) \
		::: input_size 167772160
done

) >> ${fn}.txt
