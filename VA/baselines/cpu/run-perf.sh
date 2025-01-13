#!/bin/zsh

make -B numa=1

perf stat record -o t1.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} ./va -a 4 -b 4 -c 4 -t 1 -e 20 -w 0 -i 167772160
perf stat record -o t4.perf -e ${(j:,:):-$(grep -v '^#' ../../../perf-events.txt | cut -d ' ' -f 1)} ./va -a 4 -b 4 -c 4 -t 4 -e 20 -w 0 -i 167772160
