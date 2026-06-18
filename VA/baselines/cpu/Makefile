benchmark ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
numa_memcpy ?= 0
perf_lib ?= 0

CFLAGS = -DDFATOOL_TIMING=1
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

.PHONY: all
all: va

TYPE ?= int32_t

va: app_baseline.c
	gcc -Wall -Wextra -pedantic ${CFLAGS} -O3 -o va -fopenmp -DNUMA=${numa} -DNUMA_MEMCPY=${numa_memcpy} -DNOP_SYNC=${nop_sync} -DWITH_BENCHMARK=${benchmark} -DT=${TYPE} -DWITH_PERF_LIB=${perf_lib} app_baseline.c ${LDFLAGS}

.PHONY: run
run: va
	./va -t 4

# -w/-e are not part of upstream config

.PHONY: clean
clean:
	rm -f va
