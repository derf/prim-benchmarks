#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/gemv_host -w 0 -e 5 -n ${nr_cols} -m ${nr_rows} 2>&1
}

export -f run_benchmark_nmc

fn=log/$(hostname)/ccmcc25-sdk${sdk}-sim

source ~/lib/local/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh simulator

echo "prim-benchmarks  GEMV  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 nr_cols={nr_cols} nr_rows={nr_rows} \
	::: nr_dpus 1 2 4 8 16 32 48 64 \
	::: nr_cols 256 512 768 1024 1536 \
	::: nr_rows 512 68 1024 1536 2048 \
>> ${fn}.txt
