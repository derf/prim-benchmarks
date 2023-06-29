#!/bin/bash

set -e

for i in 1 2 4 8 16 32 48 64 80 96 112 128 160 192 224 256 320 384 448 512; do
	for j in $(seq 0 32); do
		./make-size.sh $j
		n_nops=$((j * 128))
		if make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 DPU_BINARY=\'\"bin/dpu_size\"\'; then
			for l in $(seq 1 40); do
				bin/host_code -w 1 -e 0 -x 1 -i 65536 -N $n_nops || true
			done
		fi
	done
done

./make-size.sh 0

for i in 1 2 4 8 16 32 48 64 80 96 112 128 160 192 224 256 320 384 448 512; do
	for j in 1; do
		for k in SERIAL PUSH BROADCAST; do
			# 8 B ... 64 MB
			for l in 1 4 16 64 256 1024 4096 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 6291456 838868; do
				make -B NR_DPUS=$i NR_TASKLETS=$j BL=10 TRANSFER=$k
				bin/host_code -w 0 -e 50 -x 1 -i $l || true
			done
		done
	done
done
