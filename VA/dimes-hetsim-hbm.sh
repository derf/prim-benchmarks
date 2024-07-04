#!/bin/bash

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)-baseline

# upstream uses 167772160 * int32 == 1.25 GiB for DPU version

run_benchmark() {
	eval "$@"
	./va -i ${input_size} -a ${ram} -b ${ram} -c ${cpu} -t ${nr_threads} -w 0 -e 40
	return $?
}

export -f run_benchmark

(

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark nr_threads={nr_threads} ram={ram} cpu={cpu} \
	input_size={input_size} \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15) \
	::: input_size 167772160

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark nr_threads={nr_threads} ram={ram} cpu={cpu} \
	input_size={input_size} \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram $(seq 0 15) \
	::: input_size 167772160

) | tee ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
