#!/bin/sh

set -e

echo "prim-benchmarks SEL CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

for nr_threads in 1 2 4 6 8 12 16 20 24 32; do
	for i in 1258291200 629145600 314572800 157286400 78643200 39321600 19660800; do
		for dt in uint8_t uint16_t uint32_t uint64_t float double; do
			if make -B TYPE=${dt}; then
				timeout -k 1m 30m ./sel -i ${i} -w 0 -e 100 -t ${nr_threads} || true
			fi
		done
	done
done
