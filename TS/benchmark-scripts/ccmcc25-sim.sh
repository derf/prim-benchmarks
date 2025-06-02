#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=8 \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/ts_host -w 0 -e 5 -n ${ts_size} 2>&1
}

export -f run_benchmark_nmc

fn=log/$(hostname)/ccmcc25-sim

source ~/lib/local/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh simulator

echo "prim-benchmarks  TS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=8 ts_size={ts_size} \
	::: nr_dpus 1 2 4 8 16 32 48 64 \
	::: ts_size 2048 4096 8192 16384 32768 \
>> ${fn}.txt
