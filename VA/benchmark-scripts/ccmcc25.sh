#!/bin/bash

mkdir -p log/$(hostname)
fn=log/$(hostname)/ccmcc25

source /opt/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 \
	dfatool_timing=0 aspectc=1 aspectc_timing=1
	bin/host_code -w 0 -e 50 -i ${input_size}
}

export -f run_benchmark_nmc

parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 input_size={input_size} numa_rank={numa_rank} \
	::: numa_rank any \
	::: nr_dpus 64 128 256 512 768 1024 1536 2048 2304 \
	::: input_size 83886080 167772160 335544320 671088640 \
>> ${fn}.txt
