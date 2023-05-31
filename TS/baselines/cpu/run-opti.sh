#!/bin/sh

HOST="$(hostname)"

echo $HOST

make clean

for i in $(seq 1 50); do
	( make run_O0 || OMP_NUM_THREADS=32 make run_O0 ) | sed 's/CPU/CPU O0/'
done | tee "${HOST}-O0.txt"

for i in $(seq 1 50); do
	( make run_O2 || OMP_NUM_THREADS=32 make run_O2 ) | sed 's/CPU/CPU O2/'
done | tee "${HOST}-O2.txt"
