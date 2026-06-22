dfatool_timing ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
perf_lib ?= 0

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

ifeq (${perf_lib}, 1)
	CFLAGS += -I${HOME}/var/source/perf-api/build/include
	LDFLAGS += ${HOME}/var/source/perf-api/build/lib/perf/libperf.a -lcap
endif

all: mlp_openmp

mlp_openmp: mlp_openmp.c
	gcc -Wall -Wextra -pedantic -O3 ${CFLAGS} mlp_openmp.c -o mlp_openmp -DNUMA=${numa} -DNOP_SYNC=${nop_sync} -DDFATOOL_TIMING=${dfatool_timing} -DWITH_PERF_LIB=${perf_lib} -fopenmp -std=c99 ${LDFLAGS}

run: mlp_openmp
	./mlp_openmp

clean:
	rm -f mlp_openmp

.PHONY: all run clean
