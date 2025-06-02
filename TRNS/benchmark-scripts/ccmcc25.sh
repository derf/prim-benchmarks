#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} dfatool_timing=0 aspectc=1 aspectc_timing=1
	bin/host_code -w 0 -e 4 -p ${cols} -o ${rows} -m ${tile_rows} -n ${tile_cols}
}

export -f run_benchmark_nmc

for sdk in 2023.2.0 2024.1.0 2024.2.0 2025.1.0; do

	fn=log/$(hostname)/ccmcc25-sdk${sdk}

	source /opt/upmem/upmem-${sdk}-Linux-x86_64/upmem_env.sh

	echo "prim-benchmarks  TRNS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" >> ${fn}.txt

	parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
		run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank=any cols={cols} rows={rows} tile_cols={tile_cols} tile_rows={tile_rows} \
		::: nr_dpus 64 128 256 512 768 1024 1536 2048 2304 \
		::: rows 1024 2048 4096 \
		::: cols 64 128 256 512 768 1024 1536 2048 2304 \
		::: tile_rows 16 \
		::: tile_cols 8 \
	>> ${fn}.txt

done
