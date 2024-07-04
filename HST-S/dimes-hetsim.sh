#!/bin/sh

cd baselines/cpu
make -B NUMA=1

# upstream uses 1006632960 uint32 elements == 3.75 GiB
# This is way larger than the input file...

for i in `seq 1 10`; do
	for nr_threads in 1 2 4 8 12 16; do
		for cpu in 0 1 2 3 4 5 6 7; do
			for ram in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
				for j in `seq 1 10`; do
					./hist -A $ram -B $ram -C $cpu -t $nr_threads -w 0 -e 100 -x 2
				done
				./hist -i 1006632960 -A $ram -B $ram -C $cpu -t $nr_threads -w 0 -e 20 -x 2
			done
		done
	done
done
