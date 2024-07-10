NUMA ?= 0
FLAGS =

ifeq (${NUMA}, 1)
	FLAGS += -lnuma
endif

DTYPE ?= double

all: streamp_openmp

streamp_openmp: streamp_openmp.cpp tools.cpp
	g++ -ggdb -Wall -Wextra -pedantic -march=native -O2 -DNUMA=${NUMA} -DDTYPE=${DTYPE} streamp_openmp.cpp tools.cpp -o streamp_openmp -std=c++11 -fopenmp ${FLAGS}

streamp_openmp_O0: streamp_openmp.cpp tools.cpp
	g++ streamp_openmp.cpp tools.cpp -o streamp_openmp_O0 -std=c++11 -fopenmp

streamp_openmp_O2: streamp_openmp.cpp tools.cpp
	g++ -O2 streamp_openmp.cpp tools.cpp -o streamp_openmp_O2 -std=c++11 -fopenmp

run: streamp_openmp
	./streamp_openmp inputs/randomlist33M.txt 256

# may need OMP_NUM_THREADS=32 (≈ tinos) -- does not work with 88 threads @ ios

run_O0: streamp_openmp_O0
	./streamp_openmp_O0 inputs/randomlist33M.txt 256

run_O2: streamp_openmp_O2
	./streamp_openmp_O2 inputs/randomlist33M.txt 256

clean:
	rm -f streamp_openmp streamp_openmp_O0 streamp_openmp_O2

inputs/randomlistDPU.txt: inputs/randomlist33M.txt
	head -n 33554432 $^ > $@

inputs/randomlist10M.txt: inputs/randomlist33M.txt
	head -n 10000000 $^ > $@

.PHONY: all run run_O0 run_O2 clean
