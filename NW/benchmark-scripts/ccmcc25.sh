#!/bin/bash

# Generates huge logfiles and crashes frequently. Probably not helpful.
exit 1

mkdir -p log/$(hostname)
fn=log/$(hostname)/ccmcc25

source /opt/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=32 BL_IN=2 \
	dfatool_timing=0 aspectc=1 aspectc_timing=1
	bin/nw_host -w 0 -e 50 -n ${nr_rows}
}

export -f run_benchmark_nmc

echo "prim-benchmarks  NW  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} nr_rows={nr_rows} \
	::: numa_rank any \
	::: nr_dpus 64 128 256 512 768 1024 1536 2048 2304 \
	::: nr_rows 32768 65536 131072 \
>> ${fn}.txt
