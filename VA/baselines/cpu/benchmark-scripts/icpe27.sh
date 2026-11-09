#!/bin/bash

# paros

make -B aspectc=1 numa=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/"$(basename -s .sh "$0")"

echo "prim-benchmarks  VA  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./va -w 1 -e 5 -i {input_size} -t {nr_threads} -A {numa_data_in} -B {numa_data_out} -C {numa_compute} \
		::: input_size $(( 2 ** 28 ))  $(( 2 ** 29 )) $(( 2 ** 30 )) \
		::: nr_threads 1 2 4 8 \
		::: numa_data_in $(seq 0 3) \
		::: numa_data_out $(seq 0 3) \
		::: numa_compute $(seq 0 3) \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
