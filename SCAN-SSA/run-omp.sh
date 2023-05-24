#!/bin/bash

set -e

# T: data type
# -w: number of un-timed warmup iterations
# -e: number of timed iterations
# -i: input size (number of elements, not number of bytes!)

echo "prim-benchmarks SCAN-SSA (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_threads in 1 2 4 6 8 12 16 20 24 32; do
	for i in 2048 4096 8192 16384 65536 262144 1048576 3932160 15728640 31457280; do
		for dt in UINT32 UINT64 INT32 INT64 FLOAT DOUBLE; do
			echo
			if make -B TYPE=${dt} bin/omp_code; then
				OMP_NUM_THREADS=$nr_threads timeout -k 1m 30m bin/omp_code -w 0 -e 100 -i ${i} || true
			fi
		done
	done
done
