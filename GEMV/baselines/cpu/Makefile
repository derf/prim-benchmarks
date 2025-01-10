native ?= 1
numa ?= 0
numa_memcpy ?= 0

CFLAGS =
LDFLAGS =
TYPE ?= double

ifeq (${native}, 1)
	CFLAGS += -march=native
endif

ifeq (${numa}, 1)
	LDFLAGS += -lnuma
endif

.PHONY: all
all: gemv

gemv: gemv_openmp.c
	gcc -ggdb -Wall -Wextra -pedantic ${CFLAGS} -O3 -o gemv -fopenmp -DNUMA=${numa} -DNUMA_MEMCPY=${numa_memcpy} -DT=${TYPE} -DTYPE_${TYPE} gemv_openmp.c ${LDFLAGS}

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
