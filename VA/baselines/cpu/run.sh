#!/bin/sh

set -e

HOST="$(hostname)"

echo $HOST

(

echo "prim-benchmarks VA CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# default input size: 16777216
# default threads: 4
# default type: int32_t

for nr_threads in 88 64 44 1 2 4 6 8 12 16 20 24 32; do
	#for i in 16777216 8388608 4194304 33554432 67108864; do
		#for dt in int8_t int16_t int32_t int64_t float double; do
		for dt in int32_t; do
			if make -B TYPE=${dt}; then
				# -w 1 to make sure that target array (C) is allocated
				timeout -k 1m 30m ./va -w 1 -e 100 -t ${nr_threads} -x 1 || true
				sleep 10
			fi
		done
	#done
done
) | tee "${HOST}-explore.txt"
