#!/bin/sh

mkdir -p "$(hostname)-alloc"

NCORES=$(grep -c '^processor' /proc/cpuinfo)
cleanexit() {
	pkill -f "stress -c ${NCORES}"
	xz -f -v -9 -M 800M "$(hostname)-alloc/rank-stress-c${NCORES}.txt"
	exit 0
}

trap cleanexit TERM INT

stress -c ${NCORES} &

./run-alloc-rank.sh | tee "$(hostname)-alloc/rank-stress-c${NCORES}.txt"

cleanexit
