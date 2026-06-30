dfatool_timing ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
numa_memcpy ?= 0
perf_lib ?= 0

CFLAGS =
LDFLAGS =
TYPE ?= double

ifeq (${debug}, 1)
	CFLAGS += -ggdb
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

gemv: gemv_openmp.c
	gcc -Wall -Wextra -pedantic -O3 ${CFLAGS} -o gemv -fopenmp -DDFATOOL_TIMING=${dfatool_timing} -DNUMA=${numa} -DNUMA_MEMCPY=${numa_memcpy} -DWITH_PERF_LIB=${perf_lib} -DNOP_SYNC=${nop_sync} -DT=${TYPE} -DTYPE_${TYPE} gemv_openmp.c ${LDFLAGS}

.PHONY: run

run: gemv
	./gemv

.PHONY: clean
clean:
	rm -f gemv
