#!/bin/sh

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# DPU version: -m m -n n -o M_ -p N_
# input size: sizeof(T) * M_ * m * N_ * n
# Upstream DPU version uses int64_t, -p 2048 -o 12288 -x 1 [implicit -m 16 -n 8]
# Upstream CPU version uses double,  -p 2556 -o 4096 -m 16 -n 8 and fails with -o 12288 (allocation error)
#
# Here: -p 2048 -o 2048 -m 16 -n 8 for both

(

echo "CPU single-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --header : \
	./trns -w 0 -r 40 -p 2048 -o 2048 -m 16 -n 8 -t {nr_threads} -a {ram} -b {ram} -c {cpu} \
	::: ram $(seq 0 15) \
	::: cpu $(seq 0 7) \
	::: nr_threads 1 2 4 8 12 16

echo "CPU multi-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --header : \
	./trns -w 0 -r 40 -p 2048 -o 2048 -m 16 -n 8 -t {nr_threads} -a {ram} -b {ram} -c {cpu} \
	::: ram $(seq 0 15) \
	::: cpu -1 \
	::: nr_threads 32 48 64 96 128

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
