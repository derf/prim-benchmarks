#!/bin/sh

ts="$(date +%Y%m%d)"

mkdir -p "$(hostname)-transfer"

./run-transfer-rank.sh | tee "$(hostname)-transfer/${ts}-idle.txt"

xz -f -v -9 -M 800M "$(hostname)-transfer/${ts}-idle.txt"
