#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-nmc

# 2^24 elem == 128 MiB
# 2^28 elem == 2 GiB
# (upstream version uses 1.875 GiB)
# upstrem DPU and upstream CPU use uint64_t

source /opt/upmem/upmem-2024.2.0-Linux-x86_64/upmem_env.sh

run_benchmark_nmc() {
	local "$@"
	if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1 PARALLEL_READ=1; then
		bin/host_code -w 0 -e 40 -i ${input_size} -x 1
	fi
	return $?
}

export -f run_benchmark_nmc

(

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 input_size={input_size} \
	::: nr_dpus 64 128 256 512 768 1024 1536 2048 2304 \
	::: input_size $(( 2 ** 24 )) $(( 2 ** 25 )) $(( 2 ** 26 )) $(( 2 ** 27 )) $(( 2 ** 28 ))

) >> ${fn}.txt

cd baselines/cpu

make -B NUMA=1

(
parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./count -i {input_size} -a {ram} -c {cpu} -t {nr_threads} -w 0 -e 20 \
	::: ram 0 1 \
	::: cpu 0 1 \
	::: nr_threads 1 2 4 8 12 16 \
	::: input_size $(( 2 ** 24 )) $(( 2 ** 25 )) $(( 2 ** 26 )) $(( 2 ** 27 )) $(( 2 ** 28 ))
) >> ${fn}.txt
