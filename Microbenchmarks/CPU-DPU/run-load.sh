#!/bin/bash

trap 'pkill -f "stress -c 32"' INT

set -e

: > tinos-transfer-idle.txt
: > tinos-transfer-stress-c32.txt
: > tinos-transfer-nice-stress-c32.txt

./make-size.sh 0

for i in 1 2 4 8 16 32 48 64 80 96 112 $(seq 128 32 512); do
	for j in 1; do
		for k in SERIAL PUSH BROADCAST; do
			# 8 B ... 64 MB
			for l in 1 4 16 64 256 1024 4096 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 6291456 838868; do
				echo $i $j $k $l
				make -B NR_DPUS=$i NR_TASKLETS=$j BL=10 TRANSFER=$k
				bin/host_code -w 0 -e 50 -x 1 -i $l >> tinos-transfer-idle.txt || true

				stress -c 32 &
				sleep 2
				uptime
				bin/host_code -w 0 -e 50 -x 1 -i $l >> tinos-transfer-stress-c32.txt || true
				pkill -f 'stress -c 32'
				sleep 30

				nice stress -c 32 &
				sleep 2
				uptime
				bin/host_code -w 0 -e 50 -x 1 -i $l >> tinos-transfer-nice-stress-c32.txt || true
				pkill -f 'stress -c 32'
				sleep 30
			done
		done
	done
done

for i in $(seq 512 32 2543) 2543; do
	for j in 1; do
		for k in SERIAL PUSH BROADCAST; do
			# 1 MB ... 1024 MB
			for l in 1048576 2097152 4194304 6291456 838868 1677736 3355472 6710944 13421888; do
				echo $i $j $k $l
				make -B NR_DPUS=$i NR_TASKLETS=$j BL=10 TRANSFER=$k
				bin/host_code -w 0 -e 50 -x 1 -i $l >> tinos-transfer-idle.txt || true

				stress -c 32 &
				sleep 2
				uptime
				bin/host_code -w 0 -e 50 -x 1 -i $l >> tinos-transfer-stress-c32.txt || true
				pkill -f 'stress -c 32'
				sleep 30

				nice stress -c 32 &
				sleep 2
				uptime
				bin/host_code -w 0 -e 50 -x 1 -i $l >> tinos-transfer-nice-stress-c32.txt || true
				pkill -f 'stress -c 32'
				sleep 30
			done
		done
	done
done
