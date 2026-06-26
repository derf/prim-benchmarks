dfatool_timing ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
perf_ext ?= 0
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

TYPE ?= UINT64

scan: app_baseline.cpp
	g++ -Wall -Wextra -pedantic -O3 ${CFLAGS} app_baseline.cpp -fopenmp -DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_CPP -DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_OMP -lgomp -I/usr/local/cuda-8.0/include -lm -o scan -DDFATOOL_TIMING=${dfatool_timing} -DNUMA=${numa} -DNOP_SYNC=${nop_sync} -DWITH_PERF_LIB=${perf_lib} -D${TYPE} ${LDFLAGS}

run: scan
	./scan -i 1258291200 -t 4

clean:
	rm -f scan

.PHONY: run clean
