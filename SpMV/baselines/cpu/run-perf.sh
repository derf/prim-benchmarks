#!/bin/zsh

make -B

OMP_NUM_THREADS=1 perf stat record -o t1.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} make run
OMP_NUM_THREADS=4 perf stat record -o t4.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} make run
