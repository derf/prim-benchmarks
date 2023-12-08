#!/bin/bash

NCORES=$(grep -c '^processor' /proc/cpuinfo)

trap "pkill -f 'stress -c ${NCORES}'" INT

set -e

mkdir -p $(hostname)-alloc
rm -f $(hostname)-alloc/*

for f in $(hostname)-alloc/idle.txt $(hostname)-alloc/stress-c${NCORES}.txt $(hostname)-alloc/nice-stress-c${NCORES}.txt; do
	echo "prim-benchmarks CPU-DPU alloc (dfatool edition)" >> $f
	echo "Started at $(date)" >> $f
	echo "Revision $(git describe --always)" >> $f
done

# runtime exclusive of host_code execution time: 25 seconds per inner loop
# *16  -> about 7 minutes per outer loop
# *163 -> about 18 hours total
for i in 1 2 4 8 16 32 48 64 80 96 112 $(seq 128 16 2542) 2542; do
	for j in $(seq 0 16); do
		echo $i/2542 $j/16
		./make-size.sh $j
		n_nops=$((j * 256))
		if make -B NR_DPUS=$i NR_TASKLETS=1 BL=10 DPU_BINARY=\'\"bin/dpu_size\"\'; then

			uptime
			S_TIME_FORMAT=ISO mpstat -P ALL -N ALL -n -o JSON 1 > $(hostname)-alloc/n_dpus=${i}-n_nops=${n_nops}.json &
			for l in $(seq 1 40); do
				bin/host_code -w 1 -e 0 -x 1 -i 65536 -N $n_nops >> $(hostname)-alloc/idle.txt || true
			done
			pkill -f mpstat

			stress -c ${NCORES} &
			sleep 2
			uptime
			for l in $(seq 1 40); do
				bin/host_code -w 1 -e 0 -x 1 -i 65536 -N $n_nops >> $(hostname)-alloc/stress-c${NCORES}.txt || true
			done
			pkill -f "stress -c ${NCORES}"

			sleep 10

			nice stress -c ${NCORES} &
			sleep 2
			uptime
			for l in $(seq 1 40); do
				bin/host_code -w 1 -e 0 -x 1 -i 65536 -N $n_nops >> $(hostname)-alloc/nice-stress-c${NCORES}.txt || true
			done
			pkill -f "stress -c ${NCORES}"

			sleep 10
		fi
	done
done

for f in $(hostname)-alloc/idle.txt $(hostname)-alloc/stress-c${NCORES}.txt $(hostname)-alloc/nice-stress-c${NCORES}.txt; do
	echo "Completed at $(date)" >> $f
done

for f in $(hostname)-alloc/idle.txt $(hostname)-alloc/stress-c${NCORES}.txt $(hostname)-alloc/nice-stress-c${NCORES}.txt; do
	xz -v -9 -M 800M $f
done
