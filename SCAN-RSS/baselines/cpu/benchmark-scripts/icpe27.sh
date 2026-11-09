#!/bin/bash

# paros

make -B aspectc=1 numa=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/"$(basename -s .sh "$0")"

echo "prim-benchmarks  SCAN-RSS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./scan -w 1 -e 5 -i {nr_elements} -t {nr_threads} -A {numa_data_in} -B {numa_data_out} -C {numa_compute} \
		::: nr_elements $((2**27)) $((2**28)) $((2**29)) \
		::: nr_threads 1 2 4 8 \
		::: numa_data_in $(seq 0 3) \
		::: numa_compute $(seq 0 3) \
		::: numa_data_out $(seq 0 3) \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
