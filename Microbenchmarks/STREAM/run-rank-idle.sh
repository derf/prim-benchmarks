#!/bin/bash

mkdir -p $(hostname)

./run-rank.sh | tee "$(hostname)/$(date +%Y%m%d)-rank-idle.txt"
