#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_nmc() {
	local "$@"
	set -e
	make -B NR_RANKS=${nr_ranks} NR_TASKLETS=${nr_tasklets} \
		aspectc=1 aspectc_timing=1 dfatool_timing=0
	bin/host_code -v 0 -f data/${data} 2>&1
}

export -f run_benchmark_nmc

cd data/generate
for i in 8 32 64; do
	./replicate ../bcsstk30.mtx $i ../bcsstk30.${i}.mtx
done
cd ../..

for sdk in 2025.1.0; do

	fn=log/$(hostname)/dimes26-sdk${sdk}

	source /opt/upmem/upmem-${sdk}-Linux-x86_64/upmem_env.sh

	echo "prim-benchmarks  SpMV  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

	parallel -j1 --eta --joblog ${fn}.joblog --header : \
		run_benchmark_nmc nr_ranks={nr_ranks} nr_tasklets=16 data={data} \
		::: i $(seq 0 10) \
		::: data bcsstk30.mtx bcsstk30.8.mtx bcsstk30.32.mtx bcsstk30.64.mtx \
		::: nr_ranks 1 2 4 8 12 16 20 24 28 32 36 40 \
	>> ${fn}.txt

	xz -f -v -9 ${fn}.txt

done

rm -f data/bcsstk30.*.mtx
