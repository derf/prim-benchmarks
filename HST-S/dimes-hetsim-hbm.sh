#!/bin/bash

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# Upstream DPU version uses 256 bins and 1536 * 1024 * 64 uint32 elements == 384 MiB total (-x 2 with implicit -z 64)
input_size_upstream=$((1536 * 1024 * 64))

# Here: 2 GiB
input_size_dpu=$((2**29))

(

echo "single-node execution, upstream ref (1/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./hist -i ${input_size_upstream} -A {ram} -B {ram} -C {cpu} -t {nr_threads} -w 0 -e 5 -x 1 \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15)

echo "single-node execution, DPU ref (2/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./hist -i ${input_size_dpu} -A {ram} -B {ram} -C {cpu} -t {nr_threads} -w 0 -e 5 -x 1 \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15)

echo "multi-node execution, upstream ref (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	./hist -i ${input_size_upstream} -A {ram} -B {ram} -C {cpu} -t {nr_threads} -w 0 -e 40 -x 1 \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 15)

echo "multi-node execution, DPU ref (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	./hist -i ${input_size_dpu} -A {ram} -B {ram} -C {cpu} -t {nr_threads} -w 0 -e 40 -x 1 \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 15)

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
