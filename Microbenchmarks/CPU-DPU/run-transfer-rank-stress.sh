#!/bin/sh

mkdir -p "$(hostname)-transfer"

NCORES=$(grep -c '^processor' /proc/cpuinfo)
cleanexit() {
	pkill -f "stress -c ${NCORES}"
}

trap cleanexit TERM INT

stress -c ${NCORES} &

./run-transfer-rank.sh | tee "$(hostname)-transfer/rank-stress-c${NCORES}.txt"

cleanexit

xz -f -v -9 -M 800M "$(hostname)-transfer/rank-stress-c${NCORES}.txt"
