#!/bin/sh

HOST="$(hostname)"

echo $HOST

make clean

OMP_NUM_THREADS=4 make run_O0 | sed 's/CPU/CPU O0/' | tee "${HOST}-O0.txt"

OMP_NUM_THREADS=4 make run_O2 | sed 's/CPU/CPU O2/' | tee "${HOST}-O2.txt"
