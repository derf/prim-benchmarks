benchmark ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0

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

all: mlp_openmp

mlp_openmp: mlp_openmp.c
	gcc -Wall -Wextra -pedantic -O3 ${CFLAGS} mlp_openmp.c -o mlp_openmp -DNUMA=${numa} -DNOP_SYNC=${nop_sync} -DWITH_BENCHMARK=${benchmark} -fopenmp -std=c99 ${LDFLAGS}

mlp_openmp_O0: mlp_openmp.c
	gcc mlp_openmp.c -o mlp_openmp_O0 -fopenmp -std=c99

mlp_openmp_O2: mlp_openmp.c
	gcc -O2 mlp_openmp.c -o mlp_openmp_O2 -fopenmp -std=c99

run: mlp_openmp
	./mlp_openmp

run_O0: mlp_openmp_O0
	./mlp_openmp_O0

run_O2: mlp_openmp_O2
	./mlp_openmp_O2

clean:
	rm -f mlp_openmp mlp_openmp_O0 mlp_openmp_O2

.PHONY: all run run_O0 run_O2 clean
