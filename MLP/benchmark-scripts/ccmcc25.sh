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
	bin/mlp_host -w 0 -e 50 -m ${nr_rows} -n ${nr_cols}
}

export -f run_benchmark_nmc

echo "prim-benchmarks  MLP  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} nr_cols={nr_cols} nr_rows={nr_rows} \
	::: numa_rank any \
	::: nr_dpus 64 128 256 512 768 1024 1536 2048 2304 \
	::: nr_cols 4096 8192 16384 \
	::: nr_rows 1024 2048 4096 \
>> ${fn}.txt
