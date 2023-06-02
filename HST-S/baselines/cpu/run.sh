#!/bin/sh

set -e

HOST="$(hostname)"

echo $HOST

(
echo "prim-benchmarks HST-S CPU (dfatool edition)"
echo "Started at $(date)"
echo "Revision $(git describe --always)"

# baseline ./hist supports -x, however -x 0 references uninitialized variables
# and likely never has been used or tested. So we'll leave that out here.

make -B verbose=1

for nr_threads in 88 64 44 32 24 20 1 2 4 6 8 12 16; do
	for i in `seq 1 20`; do
		timeout --foreground -k 1m 30m ./hist -t ${nr_threads} -i 1006632960 || true
	done
done
) | tee "${HOST}-explore.txt"
