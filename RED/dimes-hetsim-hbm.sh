#!/bin/sh

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# upstream DPU version uses 419430400 * int64 == 3.125 GiB

(

echo "CPU single-node (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./red -i 419430400 -w 0 -e 5 -t 8 -x 1 -a {ram} -c {cpu} \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15) \
	::: nr_threads 1 2 4 8 12 16

echo "CPU multi-node (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./red -i 419430400 -w 0 -e 40 -t 8 -x 1 -a {ram} -c {cpu} \
	::: cpu -1 \
	::: ram $(seq 0 15) \
	::: nr_threads 32 48 64 96 128

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
