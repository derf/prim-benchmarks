#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_gpu() {
	local "$@"
	set -e
	make -B NR_THREADS=${nr_threads} aspectc=1 aspectc_timing=1
	./va ${input_size} 1 > /dev/null
	./va ${input_size} 1
}

export -f run_benchmark_gpu

fn=log/$(hostname)/dimes26

echo "prim-benchmarks  VA  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	run_benchmark_gpu nr_threads={nr_threads} input_size={input_size} \
	::: nr_threads 64 128 256 \
	::: input_size 83886080 167772160 335544320 671088640 838860800 1677721600 3355443200 6710886400 \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
