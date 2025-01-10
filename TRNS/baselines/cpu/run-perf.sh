#!/bin/zsh

make -B numa=1

perf stat record -o t1.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} ./trns -w 0 -r 20 -p 2048 -o 2048 -m 16 -n 8 -t 1 -a 4 -c 4
perf stat record -o t4.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} ./trns -w 0 -r 20 -p 2048 -o 2048 -m 16 -n 8 -t 4 -a 4 -c 4
