#!/bin/sh

cd baselines/cpu
make -B NUMA=1

# upstream uses 16777216 * int32 == 64 MiB by default
# 2^29 elements * int32 == 2 GiB

for nr_threads in 1 2 4 8 12 16; do
	for cpu in 0 1 2 3 4 5 6 7; do
		for ram in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
			./va -a $ram -b $ram -c $cpu -t $nr_threads -w 0 -e 50
			./va -i $(perl -E 'say 2 ** 29') -a $ram -b $ram -c $cpu -t $nr_threads -w 0 -e 10
		done
	done
done

for nr_threads in 32 48 64 96 128; do
	for ram in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
		./va -a $ram -b $ram -c -1 -t $nr_threads -w 0 -e 50
		./va -i $(perl -E 'say 2 ** 29') -a $ram -b $ram -c -1 -t $nr_threads -w 0 -e 50
	done
done
