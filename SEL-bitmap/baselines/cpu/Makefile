dfatool_timing ?= 1
debug ?= 0
native ?= 1
numa ?= 0
perf_ext ?= 0
perf_lib ?= 0

TYPE ?= uint64_t

CFLAGS = -DDFATOOL_TIMING=${dfatool_timing} -DWITH_PERF_LIB=${perf_lib}
LDFLAGS =

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

sel: sel.c
	gcc -Wall -Wextra -pedantic -O3 ${CFLAGS} -o sel -fopenmp -DT=${TYPE} -DNUMA=${numa} sel.c ${LDFLAGS}

.PHONY: clean
	rm -f sel
