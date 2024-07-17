#!/bin/sh

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-hbm

# upstream uses 167772160 * 2 * int32 == 2.5 GiB input and 1.25 GiB output for DPU version

(

echo "single-node execution (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./va -i {input_size} -a {ram} -b {ram} -c {cpu} -t {nr_threads} -w 0 -e 5 \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15) \
	::: input_size 167772160

echo "multi-node execution (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./va -i {input_size} -a {ram} -b {ram} -c {cpu} -t {nr_threads} -w 0 -e 40 \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 15) \
	::: input_size 167772160

) >> ${fn}.txt
