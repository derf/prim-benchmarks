#!/bin/sh

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)-baseline.txt

# gemv hardcodes 167772160 double elements → 1.25 GiB of data

(

for i in `seq 1 20`; do
	for nr_threads in 1 2 4 8 12 16; do
		for cpu in 0 1 2 3 4 5 6 7; do
			for ram in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
				OMP_NUM_THREADS=$nr_threads ./gemv $ram $ram $cpu
			done
		done
	done
	for nr_threads in 32 48 64 96 128; do
		for ram in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
				OMP_NUM_THREADS=$nr_threads ./gemv $ram $ram -1
		done
	done
done

) | tee $fn

xz -f -v -9 -M 800M $fn
