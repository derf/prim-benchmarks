#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)

# upstream uses 251658240 * INT64 == 1.875 GiB

run_benchmark_nmc() {
	local "$@"
	sudo limit_ranks_to_numa_node ${numa_rank}
	if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
		bin/host_code -w 0 -e 100 -i ${input_size} -x 1
	fi
	return $?
}

export -f run_benchmark_nmc

(

echo "NMC single-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 input_size={input_size} numa_rank={numa_rank} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024 \
	::: input_size 251658240

echo "NMC multi-node operation (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 input_size={input_size} numa_rank={numa_rank} \
	::: numa_rank any \
	::: nr_dpus 1536 2048 2304 \
	::: input_size 251658240

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt

cd baselines/cpu
make -B NUMA=1

(

echo "CPU single-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./scan -i {input_size} -a {ram} -b {ram} -c {cpu} -t {nr_threads} -w 0 -e 40 -x 1 \
	::: ram 0 1 \
	::: cpu 0 1 \
	::: nr_threads 1 2 4 8 12 16 32 \
	::: input_size 251658240

echo "CPU multi-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./scan -i {input_size} -a {ram} -b {ram} -c {cpu} -t {nr_threads} -w 0 -e 40 -x 1 \
	::: ram 0-1 \
	::: cpu -1 \
	::: nr_threads 48 64 \
	::: input_size 251658240

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
