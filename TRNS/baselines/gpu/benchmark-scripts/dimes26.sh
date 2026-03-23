#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_gpu() {
	local "$@"
	set -e
	make -B aspectc=1 aspectc_timing=1
	./trns -w 0 -r 1 -p ${cols} -o ${rows} -m ${tile_rows} -n ${tile_cols} > /dev/null
	./trns -w 0 -r 1 -p ${cols} -o ${rows} -m ${tile_rows} -n ${tile_cols}
}

export -f run_benchmark_gpu

fn=log/$(hostname)/dimes26

echo "prim-benchmarks  TRNS  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	run_benchmark_gpu cols={cols} rows={rows} tile_cols={tile_cols} tile_rows={tile_rows} \
	::: rows 128 256 512 1024 2048 4096 \
	::: cols 128 256 512 768 1024 1536 2048 4096 \
	::: tile_rows 16 \
	::: tile_cols 8 \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
