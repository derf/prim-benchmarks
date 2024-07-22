#!/bin/sh

cd baselines/cpu

mkdir -p log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-hbm

# upstream uses 167772160 * 2 * int32 == 2.5 GiB input and 1.25 GiB output for DPU version

(

make -B NUMA=1 NUMA_MEMCPY=1

echo "CPU single-node operation with setup cost, memcpy node == input node, cpu node == output node (1/4)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -C {ram_local} -M {cpu_memcpy} -t {nr_threads} -w 0 -e 20 \
	::: nr_threads 1 2 4 8 12 16 \
	:::      ram_in $(seq 0 15) \
	:::+ cpu_memcpy $(seq 0 7) $(seq 0 7) \
	:::   ram_local $(seq 0 15) \
	:::+        cpu $(seq 0 7) $(seq 0 7) \
	:::+    ram_out $(seq 0 15) \
	::: input_size 167772160

make -B NUMA=1

echo "single-node execution, cpu/out on same node (2/4)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -t {nr_threads} -w 0 -e 5 \
	::: nr_threads 1 2 4 8 12 16 \
	:::   ram_in $(seq 0 15) \
	:::      cpu $(seq 0 7) $(seq 0 7) \
	:::+ ram_out $(seq 0 15) \
	::: input_size 167772160

echo "single-node execution, in/out on same node (3/4)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -t {nr_threads} -w 0 -e 5 \
	::: nr_threads 1 2 4 8 12 16 \
	::: cpu $(seq 0 7) \
	:::   ram_in $(seq 0 15) \
	:::+ ram_out $(seq 0 15) \
	::: input_size 167772160

echo "multi-node execution (4/4)" >&2

parallel -j1 --eta --joblog ${fn}.4.joblog --resume --header : \
	./va -i {input_size} -a {ram_in} -b {ram_out} -c {cpu} -t {nr_threads} -w 0 -e 20 \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram_in $(seq 0 15) \
	::: ram_out $(seq 0 15) \
	::: input_size 167772160

) >> ${fn}.txt
