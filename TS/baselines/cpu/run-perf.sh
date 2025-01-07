#!/bin/zsh

make -B NUMA=1

for i in $(seq 1 20); do
	OMP_NUM_THREADS=1 perf stat record -o t1.${i}.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} ./streamp_openmp inputs/randomlist10M.txt 256 4 4
	OMP_NUM_THREADS=4 perf stat record -o t4.${i}.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} ./streamp_openmp inputs/randomlist10M.txt 256 4 4
done
