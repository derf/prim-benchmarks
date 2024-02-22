#!/bin/sh

mkdir -p "$(hostname)-transfer"

./run-transfer-rank.sh | tee "$(hostname)-transfer/rank-idle.txt"

xz -f -v -9 -M 800M "$(hostname)-transfer/rank-idle.txt"
