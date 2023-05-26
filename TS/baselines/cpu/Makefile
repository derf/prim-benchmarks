
all: streamp_openmp

streamp_openmp: streamp_openmp.cpp tools.cpp
	g++ -O2 streamp_openmp.cpp tools.cpp -o streamp_openmp -std=c++11 -fopenmp

streamp_openmp_O0: streamp_openmp.cpp tools.cpp
	g++ streamp_openmp.cpp tools.cpp -o streamp_openmp_O0 -std=c++11 -fopenmp

streamp_openmp_O2: streamp_openmp.cpp tools.cpp
	g++ -O2 streamp_openmp.cpp tools.cpp -o streamp_openmp_O2 -std=c++11 -fopenmp

run: streamp_openmp
	./streamp_openmp inputs/randomlist33M.txt 256

run_O0: streamp_openmp_O0
	./streamp_openmp_O0 inputs/randomlist33M.txt 256

run_O2: streamp_openmp_O2
	./streamp_openmp_O2 inputs/randomlist33M.txt 256

.PHONY: all run run_O0 run_O2 clean
