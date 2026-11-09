#!/bin/bash

# paros

make -B aspectc=1 numa=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/"$(basename -s .sh "$0")"

echo "prim-benchmarks  BS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./bs_omp -w 1 -e 5 -i {nr_elements} -q {nr_queries} -t {nr_threads} -A {numa_in} -C {numa_cpu} \
		::: nr_threads 1 2 4 8 \
		::: numa_in 0 1 2 3 \
		::: numa_cpu 0 1 2 3 \
		::: nr_elements $((2**20)) $((2**21)) $((2**22)) $((2**23)) $((2**24)) $((2**25)) $((2**26)) \
		::: nr_queries 524288 1048576 2097152 16777216 \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
