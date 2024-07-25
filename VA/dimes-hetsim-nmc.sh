#!/bin/bash

mkdir -p log/$(hostname) baselines/cpu/log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-nmc

source /opt/upmem/upmem-2024.1.0-Linux-x86_64/upmem_env.sh

# upstream uses 167772160 * 2 * int32 == 2.5 GiB input and 1.25 GiB output for DPU version

run_benchmark_nmc() {
	local "$@"
	set -e
	sudo limit_ranks_to_numa_node ${numa_rank}
	make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1
	bin/host_code -w 0 -e 40 -i ${input_size} -x 1
}

export -f run_benchmark_nmc

(

echo "NMC single-node operation (1/2)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 input_size={input_size} numa_rank={numa_rank} \
	::: numa_rank 0 1 \
	::: nr_dpus 64 128 256 512 768 1024 \
	::: input_size 167772160

echo "NMC multi-node operation (2/2)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	run_benchmark_nmc nr_dpus={nr_dpus} nr_tasklets=16 input_size={input_size} numa_rank={numa_rank} \
	::: numa_rank any \
	::: nr_dpus 1536 2048 2304 \
	::: input_size 167772160

) >> ${fn}.txt

cd baselines/cpu
make -B NUMA=1

(

echo "CPU single-node operation (1/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -t {nr_threads} -w 0 -e 20 \
	::: ram_in 0 1 \
	::: ram_out 0 1 \
	::: cpu 0 1 \
	::: nr_threads 1 2 4 8 12 16 \
	::: input_size 167772160

echo "CPU multi-node operation (2/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -t {nr_threads} -w 0 -e 20 \
	::: ram_in 0 1 \
	::: ram_out 0 1 \
	::: cpu -1 \
	::: nr_threads 24 32 \
	::: input_size 167772160

) >> ${fn}.txt

make -B NUMA=1 NUMA_MEMCPY=1

(

echo "CPU single-node operation with setup cost, memcpy node == input node, cpu node == output node (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -C {cpu} -M {cpu_memcpy} -t {nr_threads} -w 0 -e 20 \
	:::      ram_in 0 1 \
	:::+ cpu_memcpy 0 1 \
	:::      cpu 0 1 \
	:::+ ram_out 0 1 \
	::: nr_threads 1 2 4 8 12 16 \
	::: input_size 167772160

echo "CPU single-node operation with setup cost, memcpy node == 0, cpu node == output node (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -C {cpu} -M {cpu_memcpy} -t {nr_threads} -w 0 -e 20 \
	::: ram_in 0 1 \
	::: cpu_memcpy 0 \
	:::      cpu 0 1 \
	:::+ ram_out 0 1 \
	::: nr_threads 1 2 4 8 12 16 \
	::: input_size 167772160

) >> ${fn}.txt
