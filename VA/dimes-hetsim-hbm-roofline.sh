#!/bin/sh

cd baselines/cpu
make -B NUMA=1

mkdir -p log/$(hostname)
fn=log/$(hostname)/dimes-hetsim-roofline

# upstream uses 167772160 * 2 * int32 == 2.5 GiB input and 1.25 GiB output for DPU version

(

for i in $(seq 0 7); do
	echo "single-node execution ($i/8)" >&2
	parallel -j1 --eta --joblog ${fn}.joblog --resume --header : \
		./va -i {input_size} -a {ram} -b {ram} -c {cpu} -t {nr_threads} -w 0 -e 5 \
		::: nr_threads $(seq 1 16) \
		::: cpu $i \
		::: ram $i $((i+8)) \
		::: input_size 167772160
done

) >> ${fn}.txt
