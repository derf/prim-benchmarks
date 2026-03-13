#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	make -B NR_RANKS=${nr_ranks} NR_TASKLETS=${nr_tasklets} \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/gemv_host -w 0 -e 10 -n ${nr_cols} -m ${nr_rows} 2>&1
}

export -f run_benchmark_nmc

for sdk in 2025.1.0; do

	fn=log/$(hostname)/dimes26-sdk${sdk}

	source /opt/upmem/upmem-${sdk}-Linux-x86_64/upmem_env.sh

	echo "prim-benchmarks  GEMV  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

	parallel -j1 --eta --joblog ${fn}.joblog --header : \
		run_benchmark_nmc nr_ranks={nr_ranks} nr_tasklets=16 nr_cols={nr_cols} nr_rows={nr_rows} \
		::: nr_ranks 1 2 4 8 12 16 20 24 28 32 36 40 \
		::: nr_cols 2048 4096 8192 \
		::: nr_rows 40960 81920 163840 \
	>> ${fn}.txt

	xz -f -v -9 ${fn}.txt

done
