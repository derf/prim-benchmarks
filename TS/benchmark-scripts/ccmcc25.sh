#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=8 \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/ts_host -w 0 -e 50 -n ${ts_size} 2>&1
}

export -f run_benchmark_nmc

for sdk in 2023.2.0 2024.1.0 2024.2.0 2025.1.0; do

	fn=log/$(hostname)/ccmcc25-sdk${sdk}

	source /opt/upmem/upmem-${sdk}-Linux-x86_64/upmem_env.sh

	echo "prim-benchmarks  TS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

	parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
		run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=8 numa_rank=any ts_size={ts_size} \
		::: nr_dpus 64 128 256 512 768 1024 1536 2048 2304 \
		::: ts_size 8388608 16777216 33554432 67108864 \
	>> ${fn}.txt

done
