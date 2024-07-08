#!/bin/sh

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# upstream uses 251658240 * INT64 == 1.875 GiB

(

echo "single-node execution (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./scan -i {input_size} -a {ram} -b {ram} -c {cpu} -t {nr_threads} -w 0 -e 5 -x 1 \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15) \
	::: input_size 251658240

echo "multi-node execution (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./scan -i {input_size} -a {ram} -b {ram} -c {cpu} -t {nr_threads} -w 0 -e 40 -x 1 \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 15) \
	::: input_size 251658240

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
