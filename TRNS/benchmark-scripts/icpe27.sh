#!/bin/bash

source /opt/upmem/upmem-2025.1.0-Linux-x86_64/upmem_env.sh

run_benchmark_nmc() {
	local "$@"
	set -e
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} \
		dfatool_timing=0 aspectc=1 aspectc_timing=1
	bin/host_code -w 1 -e 5 -p ${cols} -o ${rows} -m ${tile_rows} -n ${tile_cols}
}

export -f run_benchmark_nmc

mkdir -p log/$(hostname)
fn=log/$(hostname)/"$(basename -s .sh "$0")"

echo "prim-benchmarks  TRNS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 cols={cols} rows={rows} tile_cols={tile_cols} tile_rows={tile_rows} \
		::: nr_dpus 64 128 256 512 768 1024 1536 2048 2304 \
		::: rows 32 64 128 \
		::: cols 32 64 128 \
		::: tile_rows 16 \
		::: tile_cols 8 \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
