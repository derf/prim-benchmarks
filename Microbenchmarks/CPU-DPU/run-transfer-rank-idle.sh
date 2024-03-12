#!/bin/sh

mkdir -p "$(hostname)-transfer"

ts="$(date +%Y%m%d)"

./run-transfer-rank.sh | tee "$(hostname)-transfer/${ts}-rank-idle.txt"

xz -f -v -9 -M 800M "$(hostname)-transfer/${ts}-rank-idle.txt"
