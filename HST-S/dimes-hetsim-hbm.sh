#!/bin/sh

cd baselines/cpu

mkdir -p log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-hbm

# Input: (2^29 == 536870912) * int32 == 2 GiB

(

make -B NUMA=1 NUMA_MEMCPY=1

echo "CPU single-node operation with setup cost, memcpy node == input node, cpu node == output node (1/3)" >&2

parallel -j1 --eta --joblog ${fn}.1.joblog --resume --header : \
	./hist -i {input_size} -b 256 -A {ram_in} -B {ram_out} -C {cpu} -D {ram_local} -M {cpu-memcpy} -t {nr_threads} \
	::: i $(seq 1 5) \
	::: nr_threads 1 2 4 8 12 16 \
	:::      ram_in $(seq 0 15) \
	:::+ cpu_memcpy $(seq 0 7) $(seq 0 7) \
	:::   ram_local $(seq 0 15) \
	:::+        cpu $(seq 0 7) $(seq 0 7) \
	:::+    ram_out $(seq 0 15) \
	::: input_size 536870912

make -B NUMA=1

echo "single-node execution, cpu/out on same node (2/3)" >&2

parallel -j1 --eta --joblog ${fn}.2.joblog --resume --header : \
	./hist -i {input_size} -b 256 -A {ram_in} -B {ram_out} -C {cpu} -t {nr_threads} \
	::: i $(seq 1 5) \
	::: nr_threads 1 2 4 8 12 16 \
	:::   ram_in $(seq 0 15) \
	:::      cpu $(seq 0 7) $(seq 0 7) \
	:::+ ram_out $(seq 0 15) \
	::: input_size 536870912

echo "multi-node execution (3/3)" >&2

parallel -j1 --eta --joblog ${fn}.3.joblog --resume --header : \
	./hist -i {input_size} -b 256 -A {ram_in} -B {ram_out} -C {cpu} -t {nr_threads} \
	::: i $(seq 1 20) \
	::: nr_threads 32 48 64 96 128 \
	::: cpu -1 \
	::: ram_in $(seq 0 15) \
	::: ram_out $(seq 0 15) \
	::: input_size 536870912

) >> ${fn}.txt
