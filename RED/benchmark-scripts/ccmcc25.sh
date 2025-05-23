#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/host_code -w 0 -e 5 -i ${nr_elements} 2>&1
}

export -f run_benchmark_nmc

fn=log/$(hostname)/ccmcc25-sdk${sdk}-sim

source ~/lib/local/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh simulator

echo "prim-benchmarks  BS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank=any nr_elements={nr_elements} \
		::: nr_dpus 64 128 256 512 768 1024 1536 2048 2304 \
		::: nr_elements $((2**27)) $((2**28)) $((2**29)) \
>> ${fn}.txt
