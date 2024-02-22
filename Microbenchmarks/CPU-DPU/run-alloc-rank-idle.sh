#!/bin/sh

mkdir -p "$(hostname)-alloc"

./run-alloc-rank.sh | tee "$(hostname)-alloc/rank-idle.txt"

xz -v -9 -M 800M "$(hostname)-alloc/rank-idle.txt"
