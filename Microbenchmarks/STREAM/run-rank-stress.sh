#!/bin/bash

mkdir -p $(hostname)

NCORES=$(grep -c '^processor' /proc/cpuinfo)
cleanexit() {
	pkill -f "stress -c ${NCORES}"
	exit 0
}

trap cleanexit TERM INT

stress -c ${NCORES} &

./run-rank.sh | tee "$(hostname)/$(date +%Y%m%d)-rank-stress-c${NCORES}.txt"

cleanexit
