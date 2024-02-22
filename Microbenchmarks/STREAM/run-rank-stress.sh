#!/bin/bash

mkdir -p $(hostname)

NCORES=$(grep -c '^processor' /proc/cpuinfo)
cleanexit() {
	pkill -f "stress -c ${NCORES}"
}

trap cleanexit TERM INT

stress -c ${NCORES} &

./run-rank.sh | tee "$(hostname)/rank-stress-c${NCORES}.txt"

cleanexit
