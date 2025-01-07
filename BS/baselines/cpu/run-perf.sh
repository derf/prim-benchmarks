#!/bin/zsh

make -B NUMA=1

OMP_NUM_THREADS=1 perf stat record -o t1.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} ./bs_omp $((2**29)) 16777216 4 4
OMP_NUM_THREADS=4 perf stat record -o t4.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} ./bs_omp $((2**29)) 16777216 4 4
