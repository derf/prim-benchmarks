#!/bin/sh

mkdir -p "log/dimes24-$(hostname)"

ts="$(date +%Y%m%d)"

./dimes24-hetsim-alloc.sh | tee "log/dimes24-$(hostname)/${ts}-alloc.txt"
./dimes24-hetsim-transfer.sh | tee "log/dimes24-$(hostname)/${ts}-transfer.txt"

xz -f -v -9 -M 800M "log/dimes24-$(hostname)/${ts}-alloc.txt"
xz -f -v -9 -M 800M "log/dimes24-$(hostname)/${ts}-transfer.txt"
