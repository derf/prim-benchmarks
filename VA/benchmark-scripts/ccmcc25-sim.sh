#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/host_code -w 0 -e 5 -i ${input_size}
}

export -f run_benchmark_nmc

fn=log/$(hostname)/ccmcc25-sim

source ~/lib/local/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh simulator

echo "prim-benchmarks  VA  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 input_size={input_size} \
	::: nr_dpus 1 2 4 8 16 32 48 64 \
	::: input_size 327680 655360 1310720 2621440 \
>> ${fn}.txt
