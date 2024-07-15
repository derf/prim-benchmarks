#!/bin/bash

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/$(date +%Y%m%d)-roofline

# upstream DPU version uses -m 163840 -n 4096
# → (163840 * 4096 + 4096) * uint32 ≈ 2.5 GiB

# upstream baseline uses 20480 rows and 8192 cols, allocating 20480 * double + 8192 * double + 20480 * double + 20480 * 8192 * double
# → ≈ 1.25 GiB

run_benchmark() {
	local "$@"
	OMP_NUM_THREADS=$nr_threads ./gemv $ram $ram $cpu
	return $?
}

export -f run_benchmark

(

for i in $(seq 0 7); do
	echo "single-node execution ($i/8)" >&2
	parallel -j1 --eta --joblog ${fn}.${i}.joblog --header : \
	run_benchmark nr_threads={nr_threads} ram={ram} cpu={cpu} \
	::: cpu $(seq 0 7) \
	::: ram $(seq 0 15) \
	::: nr_threads 1 2 4 8 12 16
done

) > ${fn}.txt

xz -f -v -9 -M 800M ${fn}.txt
