#!/bin/bash

mkdir -p log/$(hostname)

for sdk in 2025.1.0 2024.2.0 2024.1.0 2023.2.0; do

	fn=log/$(hostname)/fgbs26a-sdk${sdk}.txt
	: > ${fn}

	source /opt/upmem/upmem-${sdk}-Linux-x86_64/upmem_env.sh

	(
		echo "prim-benchmarks SEL (dfatool fgbs26a edition)"
		echo "Started at $(date)"
		echo "Revision $(git describe --always)"

		for nr_dpus in 2500; do # 2304 2048 2543; do
			for nr_tasklets in 16; do
				echo
				if make -B NR_DPUS=${nr_dpus} NR_TASKLETS=${nr_tasklets} BL=10 WITH_ALLOC_OVERHEAD=1 WITH_LOAD_OVERHEAD=1 WITH_FREE_OVERHEAD=1; then
					timeout --foreground -k 1m 30m bin/host_code -w 0 -e 50 -i 1258291200 -x 1 || true
				fi
			done
		done
		echo "Completed at $(date)"
	) | tee ${fn}

done
