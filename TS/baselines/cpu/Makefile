dfatool_timing ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
perf_ext ?= 0
perf_lib ?= 0

DTYPE ?= double

CFLAGS = -DDFATOOL_TIMING=${dfatool_timing} -DNOP_SYNC=${nop_sync} -DNUMA=${numa} -DDTYPE=${DTYPE} -DWITH_PERF_EXT=${perf_ext} -DWITH_PERF_LIB=${perf_lib}
LDFLAGS =

ifeq (${debug}, 1)
	CFLAGS += -g
endif

ifeq (${native}, 1)
	CFLAGS += -march=native
endif

ifeq (${numa}, 1)
	LDFLAGS += -lnuma
endif

ifeq (${perf_lib}, 1)
	CFLAGS += -I${HOME}/var/source/perf-api/build/include
	LDFLAGS += ${HOME}/var/source/perf-api/build/lib/perf/libperf.a -lcap
endif

streamp_openmp: streamp_openmp.cpp tools.cpp
	g++ -ggdb -Wall -Wextra -pedantic -O3 ${CFLAGS} streamp_openmp.cpp tools.cpp -o streamp_openmp -std=c++11 -fopenmp ${LDFLAGS}

run: streamp_openmp
	./streamp_openmp inputs/randomlist33M.txt 256

# may need OMP_NUM_THREADS=32 (≈ tinos) -- does not work with 88 threads @ ios

clean:
	rm -f streamp_openmp

inputs/randomlistDPU.txt: inputs/randomlist33M.txt
	head -n 33554432 $^ > $@

inputs/randomlist10M.txt: inputs/randomlist33M.txt
	head -n 10000000 $^ > $@

.PHONY: all run clean
