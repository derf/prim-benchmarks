#!/bin/sh

HOST="$(hostname)"

echo $HOST

make -B TYPE=UINT64 verbose=1

./red -i 1048576000 -t 4 -w 0 -e 100 | sed 's/CPU/CPU Baseline/' | tee "${HOST}-baseline.txt"
