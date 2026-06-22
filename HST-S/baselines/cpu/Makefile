dfatool_timing ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
numa_memcpy ?= 0
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

all: hist

hist: app_baseline.c
	gcc -Wall -Wextra -pedantic -O3 ${CFLAGS} -DDFATOOL_TIMING=${dfatool_timing} -DNUMA=${numa} -DNUMA_MEMCPY=${numa_memcpy} -DNOP_SYNC=${nop_sync} -DWITH_PERF_LIB=${perf_lib} -o hist -fopenmp app_baseline.c ${LDFLAGS}

run: hist
	./hist -i 1006632960 -t 4

clean:
	rm -f hist

.PHONY: all run clean
