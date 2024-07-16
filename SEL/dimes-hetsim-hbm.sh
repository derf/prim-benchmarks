#!/bin/bash

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# 2^28 elem == 2 GiB data (upstream version uses 1.875 GiB)
# upstrem DPU and upstream CPU use uint64_t

(

parallel -j1 --eta --joblog ${fn}.1.joblog --header : \
	./sel -a {ram} -b {ram} -c {cpu} -i {input_size} -t {nr_threads} -w 0 -e 5 \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15) \
	::: input_size $(( 2 ** 28 ))

parallel -j1 --eta --joblog ${fn}.2.joblog --header : \
	./sel -a {ram} -b {ram} -c {cpu} -i {input_size} -t {nr_threads} -w 0 -e 40 \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 15) \
	::: input_size $(( 2 ** 28 ))

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
