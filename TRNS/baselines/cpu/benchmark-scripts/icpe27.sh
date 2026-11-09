#!/bin/bash

# paros

make -B aspectc=1 numa=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/"$(basename -s .sh "$0")"

echo "prim-benchmarks  TRNS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./trns -w 1 -e 5 -p {cols} -o {rows} -m {tile_rows} -n {tile_cols} -t {n_threads} -A {numa_in} -C {numa_compute} \
		::: rows 1024 2048 4096 8192 \
		::: cols 32 64 128 \
		::: tile_rows 16 \
		::: tile_cols 8 \
		::: n_threads 1 2 4 8 \
		::: numa_in $(seq 0 3) \
		::: numa_compute $(seq 0 3) \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
