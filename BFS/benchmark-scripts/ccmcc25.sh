#!/bin/bash

mkdir -p log/$(hostname)
fn=log/$(hostname)/ccmcc25

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/host_code -f ${data} 2>&1
}

export -f run_benchmark_nmc

echo "prim-benchmarks  BFS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

# BFS does not support repeated kernel invocations → repeat it here
parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank=any data={data} \
	::: i $(seq 0 10) \
	::: data data/roadNet-CA.txt data/loc-gowalla_edges.txt \
	::: nr_dpus 64 128 256 512 768 1024 1536 2048 2304 \
>> ${fn}.txt
