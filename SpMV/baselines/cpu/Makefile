dfatool_timing ?= 1
native ?= 1
numa ?= 0
perf_ext ?= 0
perf_lib ?= 0

CFLAGS = -DDFATOOL_TIMING=${dfatool_timing} -DNUMA=${numa} -DWITH_PERF_EXT=${perf_ext} -DWITH_PERF_LIB=${perf_lib}
LDFLAGS =

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

spmv: app.c
	gcc -Wall -Wextra -pedantic ${CFLAGS} -O3 -o spmv -fopenmp app.c ${LDFLAGS}

run: spmv
	./spmv -f ../../data/bcsstk30.mtx

clean:
	rm -f spmv

.PHONY: run clean
