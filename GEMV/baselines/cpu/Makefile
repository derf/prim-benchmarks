NUMA ?= 0
NUMA_MEMCPY ?= 0
FLAGS =
TYPE ?= double

ifeq (${NUMA}, 1)
	FLAGS += -lnuma
endif

.PHONY: all
all: gemv

gemv: gemv_openmp.c
	gcc -ggdb -Wall -Wextra -pedantic -march=native -O2 -o gemv -fopenmp -DNUMA=${NUMA} -DNUMA_MEMCPY=${NUMA_MEMCPY} -DT=${TYPE} -DTYPE_${TYPE} gemv_openmp.c ${FLAGS}

gemv_O0: gemv_openmp.c
	gcc -o gemv_O0 -fopenmp gemv_openmp.c

gemv_O2: gemv_openmp.c
	gcc -O2 -o gemv_O2 -fopenmp gemv_openmp.c

.PHONY: run run_O0 run_O2

run: gemv
	./gemv

run_O0: gemv_O0
	./gemv_O0

run_O2: gemv_O2
	./gemv_O2

.PHONY: clean
clean:
	rm -f gemv gemv_O0 gemv_O2
