#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# upstream DPU version uses 419430400 * int64 == 3.125 GiB

run_benchmark_nmc() {
	local "$@"
	sudo limit_ranks_to_numa_node ${numa_rank}
	if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
		bin/host_code -w 0 -e 100 -i 419430400 -x 1 || true
	fi
	return $?
}

export -f run_benchmark_nmc

(

echo "NMC single-node (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024

echo "NMC multi-node (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 numa_rank={numa_rank} \
	::: numa_rank -1 \
	::: nr_dpus 1536 2048 2304

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt

cd baselines/cpu
make -B NUMA=1

(

echo "CPU single-node (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./red -i 419430400 -w 0 -e 5 -t 8 -x 1 -a {ram} -c {cpu} \
	::: cpu 0 1 \
	::: ram 0 1 \
	::: nr_threads 1 2 4 8 12 16 32

echo "CPU multi-node (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./red -i 419430400 -w 0 -e 5 -t 8 -x 1 -a {ram} -c {cpu} \
	::: cpu -1 \
	::: ram 0 1 \
	::: nr_threads 48 64

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
