#!/bin/bash

trap 'pkill -f "stress -c 32"' INT

set -e

: > tinos-idle.txt
: > tinos-stress-c32.txt
: > tinos-nice-stress-c32.txt

for i in 1 2 4 8 16 32 48 64 80 96 112 $(seq 128 32 2543) 2543; do
	for j in $(seq 0 32); do
		echo $i/2543 $j/32
		./make-size.sh $j
		n_nops=$((j * 128))
		if make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 DPU_BINARY=\'\"bin/dpu_size\"\'; then
			uptime
			for l in $(seq 1 40); do
				bin/host_code -w 1 -e 0 -x 1 -i 65536 -N $n_nops >> tinos-idle.txt || true
			done
			stress -c 32 &
			sleep 2
			uptime
			for l in $(seq 1 40); do
				bin/host_code -w 1 -e 0 -x 1 -i 65536 -N $n_nops >> tinos-stress-c32.txt || true
			done
			pkill -f 'stress -c 32'
			sleep 30
			nice stress -c 32 &
			sleep 2
			uptime
			for l in $(seq 1 40); do
				bin/host_code -w 1 -e 0 -x 1 -i 65536 -N $n_nops >> tinos-nice-stress-c32.txt || true
			done
			pkill -f 'stress -c 32'
			sleep 30
		fi
	done
done
