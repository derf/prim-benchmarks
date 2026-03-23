#!/bin/bash

mkdir -p log/$(hostname)

run_benchmark_gpu() {
	local "$@"
	set -e
	./red -i ${input_size} > /dev/null
	./red -i ${input_size}
}

export -f run_benchmark_gpu

fn=log/$(hostname)/dimes26

make -B

echo "prim-benchmarks  RED  $(git describe --all --long)  $(git rev-parse HEAD)  $(date -R)" > ${fn}.txt

parallel -j1 --eta --joblog ${fn}.joblog --header : \
	run_benchmark_gpu input_size={input_size} \
	::: i 1 2 3 4 5 \
	::: input_size $((2**20)) $((2**21)) $((2**22)) $((2**23)) $((2**24)) $((2**25)) $((2**26)) $((2**27)) $((2**28)) $((2**29)) $((2**30)) $((2**31)) \
>> ${fn}.txt

xz -f -v -9 ${fn}.txt
