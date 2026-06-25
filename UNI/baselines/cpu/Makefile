debug ?= 0
dfatool_timing ?= 1
native ?= 1
nop_sync ?= 0
numa ?= 0
perf_ext ?= 0
perf_lib ?= 0

TYPE ?= int64_t

CFLAGS = -DT=${TYPE} -DDFATOOL_TIMING=${dfatool_timing} -DNOP_SYNC=${nop_sync} -DNUMA=${numa} -DWITH_PERF_EXT=${perf_ext} -DWITH_PERF_LIB=${perf_lib}
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

uni: app_baseline.c
	gcc -Wall -Wextra -pedantic -O3 ${CFLAGS} -o uni -fopenmp app_baseline.c ${LDFLAGS}

.PHONY: run
run: uni
	./uni -i 1258291200 -t 4

.PHONY: clean
clean:
	rm -f uni
