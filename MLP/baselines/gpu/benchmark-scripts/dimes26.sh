#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_gpu() {
	local "$@"
	set -e
	make -B aspectc=1 aspectc_timing=1 NR_THREADS=${nr_threads}
	./mlp ${m} ${n} > /dev/null 2>&1
	./mlp ${m} ${n} 2>&1
}

export -f run_benchmark_gpu

fn=log/$(hostname)/dimes26

echo "prim-benchmarks  MLP  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	run_benchmark_gpu m={m} n={n} nr_threads={nr_threads} \
	::: m 1024 2048 4096 10240 20480 40960 \
	::: n 4096 8192 16384 \
	::: nr_threads 64 128 256 512 768 1024 \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
