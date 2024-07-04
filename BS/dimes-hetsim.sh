#!/bin/sh

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)-baseline.txt

(

for i in `seq 1 20`; do
	for nr_threads in 1 2 4 8 12 16; do
		for cpu in 0 1 2 3 4 5 6 7; do
			for ram in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
				# 2048576 is INPUT_SIZE for DPU version
				# 2048576 * uint64 ≈ 16 MiB (fits into cache)
				OMP_NUM_THREADS=$nr_threads ./bs_omp 2048576 16777216 $ram $cpu
				# 2^27 * uint64 == 1 GiB
				OMP_NUM_THREADS=$nr_threads ./bs_omp $(perl -E 'say 2 ** 27') 16777216 $ram $cpu
				# 2^29 * uint64 == 4 GiB
				OMP_NUM_THREADS=$nr_threads ./bs_omp $(perl -E 'say 2 ** 29') 16777216 $ram $cpu
			done
		done
	done
	for nr_threads in 32 48 64 96 128; do
		for ram in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
			# 2048576 is INPUT_SIZE for DPU version
			# 2048576 * uint64 ≈ 16 MiB (fits into cache)
			OMP_NUM_THREADS=$nr_threads ./bs_omp 2048576 16777216 $ram -1
			# 2^27 * uint64 == 1 GiB
			OMP_NUM_THREADS=$nr_threads ./bs_omp $(perl -E 'say 2 ** 27') 16777216 $ram -1
			# 2^29 * uint64 == 4 GiB
			OMP_NUM_THREADS=$nr_threads ./bs_omp $(perl -E 'say 2 ** 29') 16777216 $ram -1
		done
	done
done

) | tee $fn

xz -f -v -9 -M 800M $fn
