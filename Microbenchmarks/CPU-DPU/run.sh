#!/bin/bash

set -e

for i in 1 2 4 8 16 32 64; do
	for j in 1; do
		for k in SERIAL PUSH BROADCAST; do
			# 8 B ... 64 MB
			for l in 1 4 16 64 256 1024 4096 16384 65536 262144 1048576 4194304 838868; do
				make -B NR_DPUS=$i NR_TASKLETS=$j BL=10 TRANSFER=$k
				bin/host_code -w 0 -e 50 -i $l
			done
		done
	done
done
