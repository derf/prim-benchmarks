#!/bin/sh

: > dpu/nop.inc

for i in $(seq 1 $1); do
	for i in $(seq 1 256); do
		echo 'asm volatile("nop");' >> dpu/nop.inc
	done
done
