#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} \
		INPUT_SIZE=${nr_elements} PROBLEM_SIZE=${nr_queries} \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/bs_host -w 0 -e 5 2>&1
}

export -f run_benchmark_nmc

fn=log/$(hostname)/ccmcc25-sim

source ~/lib/local/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh simulator

echo "prim-benchmarks  BS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 nr_elements={nr_elements} nr_queries={nr_queries} \
	::: nr_dpus 1 2 4 8 16 32 48 64 \
	::: nr_elements $((2**18)) $((2**19)) $((2**20)) $((2**21)) $((2**22)) \
	::: nr_queries 512 1024 2048 4096 \
>> ${fn}.txt
