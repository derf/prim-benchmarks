#!/bin/bash

# paros

make -B aspectc=1 numa=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/"$(basename -s .sh "$0")"

echo "prim-benchmarks  TS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./streamp_openmp -w 1 -e 5 -t {n_threads} -A {numa_in} -B {numa_out} -C {numa_compute} \
		::: n_threads 1 2 4 8 \
		::: numa_in $(seq 0 3) \
		::: numa_out $(seq 0 3) \
		::: numa_compute $(seq 0 3) \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
