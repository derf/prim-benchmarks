#!/bin/bash

mkdir -p $(hostname)

./run-rank.sh | tee "$(hostname)/rank-idle.txt"
