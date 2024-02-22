#!/bin/sh

mkdir -p "$(hostname)-alloc"

NCORES=$(grep -c '^processor' /proc/cpuinfo)
cleanexit() {
	pkill -f "stress -c ${NCORES}"
}

trap cleanexit TERM INT

stress -c ${NCORES} &

./run-alloc-rank.sh | tee "$(hostname)-alloc/rank-stress-c${NCORES}.txt"

cleanexit

xz -v -9 -M 800M "$(hostname)-alloc/rank-stress-c${NCORES}.txt"
