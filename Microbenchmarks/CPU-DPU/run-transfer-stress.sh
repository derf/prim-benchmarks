#!/bin/sh

ts="$(date +%Y%m%d)"

mkdir -p "$(hostname)-transfer"

NCORES=$(grep -c '^processor' /proc/cpuinfo)
cleanexit() {
	pkill -f "stress -c ${NCORES}"
}

trap cleanexit TERM INT

stress -c ${NCORES} &

./run-transfer-rank.sh | tee "$(hostname)-transfer/${ts}-stress-c${NCORES}.txt"

cleanexit

xz -f -v -9 -M 800M "$(hostname)-transfer/${ts}-stress-c${NCORES}.txt"
