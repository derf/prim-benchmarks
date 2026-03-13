#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	make -B NR_RANKS=${nr_ranks} NR_TASKLETS=${nr_tasklets} BL=8 \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/ts_host -w 0 -e 10 -n ${ts_size} 2>&1
}

export -f run_benchmark_nmc

for sdk in 2025.1.0; do

	fn=log/$(hostname)/dimes26-sdk${sdk}

	source /opt/upmem/upmem-${sdk}-Linux-x86_64/upmem_env.sh

	echo "prim-benchmarks  TS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

	parallel -j1 --eta --joblog ${fn}.joblog --header : \
		run_benchmark_nmc nr_ranks={nr_ranks} nr_tasklets=8 ts_size={ts_size} \
		::: nr_ranks 1 2 4 8 12 16 20 24 28 32 36 40 \
		::: ts_size 8388608 16777216 33554432 67108864 \
	>> ${fn}.txt

	xz -f -v -9 ${fn}.txt
done
