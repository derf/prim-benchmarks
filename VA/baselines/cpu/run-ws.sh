#!/bin/zsh

make -B benchmark=0 debug=1 native=0 nop_sync=1 numa=1
mkdir -p log/ws

rm -f log/ws/*.ws.*(N)

set -x

for nthreads in 1 2 4 8 12 16; do
	~/var/source/valgrind/vg-in-place --tool=ws --ws-file=log/ws/t${nthreads}.ws --ws-peak-detect=yes --ws-every=50000 --ws-track-locality=yes ./va -a 4 -b 4 -c 4 -t 1 -e 20 -w 0 -i 16777216
done

for wsfile in log/ws/*.ws.*; do
	~/var/ess/aemr/dfatool/bin/extract-kernel-ws.py app_baseline.c $wsfile --output-format valgrind-ws > log/ws/kernel-${wsfile:t:r}
	~/var/ess/aemr/dfatool/bin/extract-kernel-ws.py app_baseline.c $wsfile --output-format dfatool > log/ws/kernel-${wsfile:t:r:r}.txt
done
