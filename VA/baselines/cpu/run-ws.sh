#!/bin/zsh

make -B benchmark=0 debug=1 native=0 nop_sync=1 numa=1

~/var/source/valgrind/vg-in-place --tool=ws --ws-file=t1.ws --ws-peak-detect=yes --ws-every=50000 --ws-track-locality=yes ./va -a 4 -b 4 -c 4 -t 1 -e 20 -w 0 -i 16777216
~/var/source/valgrind/vg-in-place --tool=ws --ws-file=t4.ws --ws-peak-detect=yes --ws-every=50000 --ws-track-locality=yes ./va -a 4 -b 4 -c 4 -t 4 -e 20 -w 0 -i 16777216
