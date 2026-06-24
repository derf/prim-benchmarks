dfatool_timing ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
perf_ext ?= 0
perf_lib ?= 0

type ?= int32_t

CFLAGS = -DDFATOOL_TIMING=${dfatool_timing} -DNUMA=${numa} -DNOP_SYNC=${nop_sync} -DT=${type} -DWITH_PERF_EXT=${perf_ext} -DWITH_PERF_LIB=${perf_lib}
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

va: app_baseline.c
	gcc -Wall -Wextra -pedantic ${CFLAGS} -O3 -o va -fopenmp app_baseline.c ${LDFLAGS}

.PHONY: run
run: va
	./va -t 4

# -w/-e are not part of upstream config

.PHONY: clean
clean:
	rm -f va
