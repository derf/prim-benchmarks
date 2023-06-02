#!/bin/sh

HOST="$(hostname)"

echo $HOST

make -B TYPE=UINT64 verbose=1

timeout --foreground -k 1m 30m ./scan -i 1258291200 -t 4 -w 0 -e 100 | sed 's/CPU/CPU Baseline/' | tee "${HOST}-baseline.txt"
