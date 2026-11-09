#!/bin/bash

# paros

make -B aspectc=1 numa=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/"$(basename -s .sh "$0")"

echo "prim-benchmarks  SpMV  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

cd data/generate
./replicate ../bcsstk30.mtx 64 ../bcsstk30.64.mtx
cd ../..

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	./spmv -w 1 -e 5 -f data/bcsstk30.64.mtx -t {nr_threads} -A {numa_data_in} -B {numa_data_out} -C {numa_compute} \
		::: nr_threads 1 2 4 8 \
		::: numa_data_in $(seq 0 3) \
		::: numa_data_out $(seq 0 3) \
		::: numa_compute $(seq 0 3) \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt

rm -f data/bcsstk30.64.mtx
