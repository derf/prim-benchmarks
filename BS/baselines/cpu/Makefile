benchmark ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
numa_memcpy ?= 0

CFLAGS =
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

.PHONY: all
all: bs_omp

bs_omp: bs_omp.c
	gcc -Wall -Wextra -pedantic -O3 ${CFLAGS} -DNUMA=${numa} -DNUMA_MEMCPY=${numa_memcpy} -DNOP_SYNC=${nop_sync} -DWITH_BENCHMARK=${benchmark} bs_omp.c -o bs_omp -fopenmp ${LDFLAGS}

bs_omp_O0: bs_omp.c
	gcc bs_omp.c -o bs_omp_O0 -fopenmp

bs_omp_O2: bs_omp.c
	gcc -O2 bs_omp.c -o bs_omp_O2 -fopenmp

# bs_omp performs a single iteration and must be run in a loop for proper benchmarks

.PHONY: run run_O0 run_O2
run: bs_omp
	./bs_omp 262144 16777216

run_O0: bs_omp_O0
	./bs_omp_O0 262144 16777216

run_O2: bs_omp_O2
	./bs_omp_O2 262144 16777216

.PHONY: clean
clean:
	rm -f bs_omp bs_omp_O0 bs_omp_O2
