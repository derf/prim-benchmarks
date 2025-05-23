#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/host_code -f ${data} 2>&1
}

export -f run_benchmark_nmc

fn=log/$(hostname)/ccmcc25-sdk${sdk}-sim

source ~/lib/local/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh simulator

echo "prim-benchmarks  BFS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

# BFS does not support repeated kernel invocations → repeat it here
parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 data={data} \
	::: i $(seq 0 4) \
	::: data data/roadNet-CA.txt data/loc-gowalla_edges.txt \
	::: nr_dpus 1 2 4 8 16 32 64 \
>> ${fn}.txt
