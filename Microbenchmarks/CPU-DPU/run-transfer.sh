#!/bin/bash

NCORES=$(grep -c '^processor' /proc/cpuinfo)

trap "pkill -f 'stress -c ${NCORES}'" INT

set -e

mkdir -p $(hostname)-transfer
rm -f $(hostname)-transfer/*

for f in $(hostname)-transfer/idle.txt $(hostname)-transfer/stress-c${NCORES}.txt $(hostname)-transfer/nice-stress-c${NCORES}.txt; do
	echo "prim-benchmarks CPU-DPU transfer (dfatool edition)" >> $f
	echo "Started at $(date)" >> $f
	echo "Revision $(git describe --always)" >> $f
done

./make-size.sh 0

# runtime exclusive of host_code execution time: 25 seconds per l loop
# *18 -> about 8 minutes per k loop
# *3  -> about 23 minutes per i loop
# *24 -> about 9 hours total
for i in 1 2 4 8 16 32 48 64 80 96 112 $(seq 128 32 512); do
	for k in SERIAL PUSH BROADCAST; do
		# 8 B ... 64 MB
		for l in 1 16 256 4096 16384 65536 262144 1048576 2097152 4194304 6291456 8388608; do
			echo $i/512 $k $l/8388608
			make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 TRANSFER=$k

			uptime
			S_TIME_FORMAT=ISO mpstat -P ALL -N ALL -n -o JSON 1 > $(hostname)-transfer/n_dpus=${i}-e_mode=$k-n_elements=${l}.json &
			bin/host_code -w 0 -e 100 -x 1 -i $l >> $(hostname)-transfer/idle.txt || true
			pkill -f mpstat

			stress -c ${NCORES} &
			sleep 2
			uptime
			bin/host_code -w 0 -e 100 -x 1 -i $l >> $(hostname)-transfer/stress-c${NCORES}.txt || true
			pkill -f "stress -c ${NCORES}"

			sleep 10

			stress -c ${NCORES} &
			sleep 2
			uptime
			bin/host_code -w 0 -e 100 -x 1 -i $l >> $(hostname)-transfer/nice-stress-c${NCORES}.txt || true
			pkill -f "stress -c ${NCORES}"

			sleep 10
		done
	done
done

# runtime exclusive of host_code execution time: 25 seconds per l loop
# *9 -> about 4 minutes per k loop
# *3  -> about 12 minutes per i loop
# *65 -> about 13 hours total
for i in $(seq 512 32 2542) 2542; do
	for k in SERIAL PUSH BROADCAST; do
		# 1 MB ... 1024 MB
		for l in 1048576 2097152 4194304 6291456 838868 1677736 3355472 6710944 13421888; do
			echo $i/2542 $k $l/13421888
			make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 TRANSFER=$k

			uptime
			S_TIME_FORMAT=ISO mpstat -P ALL -N ALL -n -o JSON 1 > $(hostname)-transfer/n_dpus=${i}-e_mode=$k-n_elements=${l}.json &
			bin/host_code -w 0 -e 100 -x 1 -i $l >> $(hostname)-transfer/idle.txt || true
			pkill -f mpstat

			stress -c ${NCORES} &
			sleep 2
			uptime
			bin/host_code -w 0 -e 100 -x 1 -i $l >> $(hostname)-transfer/stress-c${NCORES}.txt || true
			pkill -f "stress -c ${NCORES}"

			sleep 10

			stress -c ${NCORES} &
			sleep 2
			uptime
			bin/host_code -w 0 -e 100 -x 1 -i $l >> $(hostname)-transfer/nice-stress-c${NCORES}.txt || true
			pkill -f "stress -c ${NCORES}"

			sleep 10
		done
	done
d
for f in $(hostname)-transfer/idle.txt $(hostname)-transfer/stress-c${NCORES}.txt $(hostname)-transfer/nice-stress-c${NCORES}.txt; do
	echo "Completed at $(date)" >> $f
done

for f in $(hostname)-transfer/idle.txt $(hostname)-transfer/stress-c${NCORES}.txt $(hostname)-transfer/nice-stress-c${NCORES}.txt; do
	xz -v -9 -M 800M $f
done
