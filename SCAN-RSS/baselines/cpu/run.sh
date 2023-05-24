#!/bin/sh

set -e

echo "prim-benchmarks SCAN-RSS CPU/Thrust (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_threads in 1 2 4 6 8 12 16 20 24 32; do
	for i in 2048 4096 8192 16384 65536 262144 1048576 3932160 15728640 31457280 262144000 1258291200 2516582400; do
		for dt in UINT32 UINT64 INT32 INT64 FLOAT DOUBLE; do
			if make -B TYPE=${dt}; then
				timeout -k 1m 30m ./scan -i ${i} -w 0 -e 100 -t ${nr_threads} || true
			fi
		done
	done
done
