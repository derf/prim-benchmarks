#!/bin/sh

set -e

HOST="$(hostname)"

echo $HOST

(

echo "prim-benchmarks SCAN-RSS CPU/Thrust (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_threads in 88 64 44 1 2 4 6 8 12 16 20 24 32; do
	#for i in 2048 4096 8192 16384 65536 262144 1048576 3932160 15728640 31457280 262144000 1258291200 2516582400; do
	# 1258291200 : README
	# 251658240 : nmc strong-full
	# 3932160 * nr_threads: nmc weak
	for i in 1258291200 251658240; do
		#for dt in UINT32 UINT64 INT32 INT64 FLOAT DOUBLE; do
		for dt in UINT64; do
			if make -B TYPE=${dt} verbose=1; then
				timeout --foreground -k 1m 45m ./scan -i ${i} -w 0 -e 100 -t ${nr_threads} || true
				sleep 10
			fi
		done
	done
done
) | tee "${HOST}-explore.txt"
