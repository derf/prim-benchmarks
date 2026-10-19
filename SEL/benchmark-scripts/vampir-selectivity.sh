#!/bin/bash

cd baselines/cpu

mkdir -p log/$(hostname)
fn=log/$(hostname)/"$(basename -s .sh "$0")"

: > "${fn}.txt"

for type in int16_t int32_t int64_t; do

	make -B dfatool_timing=1 numa=1 perf_lib=0 type=${type}

# 2²⁸ * 8B == 2 GiB each for input / output
	parallel -j1 --eta --joblog ${fn}.joblog --header : \
		./sel -i {input_size} -s {selectivity} -t {nr_threads} -A {ram_in} -B {ram_out} -C {cpu} -w 1 -e 5 \
		::: nr_threads 1 2 4 8 \
		:::   ram_in $(seq 0 17) \
		:::      cpu $(seq 0 7) \
		:::  ram_out $(seq 0 17) \
		::: input_size $(( 2 ** 28 )) \
		::: selectivity 0 1 2 4 8 16 32 \
	>> "${fn}.txt"
done

xz -f -v -9 "${fn}.txt"
