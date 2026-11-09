#!/bin/bash

# paros

make -B aspectc=1 numa=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/"$(basename -s .sh "$0")"

echo "prim-benchmarks  HST-S  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./hist -w 1 -e 5 -i {in_size} -b {bins} -t {nr_threads} -A {numa_in} -B {numa_out} -C {numa_cpu} \
		::: nr_threads 1 2 4 8 \
		::: numa_in 0 1 2 3 \
		::: numa_out 0 1 2 3 \
		::: numa_cpu 0 1 2 3 \
		::: in_size $(( 2 ** 25 )) $(( 2 ** 26 )) $(( 2 ** 27 )) \
		::: bins 128 192 256 \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
