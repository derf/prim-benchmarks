aspectc ?= 0
aspectc_timing ?= 0

dfatool_timing ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
numa_memcpy ?= 0
perf ?= 0

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

ifeq (${perf}, 1)
	CFLAGS += -I${HOME}/var/source/perf-api/build/include
	LDFLAGS += ${HOME}/var/source/perf-api/build/lib/perf/libperf.a -lcap
endif

CXX=g++
CXX_FLAGS=-std=c++11 -Wall -Wextra -pedantic -DNUMA=${numa} -DNUMA_MEMCPY=${numa_memcpy} -DNOP_SYNC=${nop_sync} -DWITH_BENCHMARK=${dfatool_timing} -DASPECTC=${aspectc} -DWITH_PERF=${perf}

ifeq (${aspectc_timing}, 1)
	ASPECTC_HOST_FLAGS += -ainclude/dfatool.ah
endif

ASPECTC_HOST_FLAGS ?= -a0

ifeq (${aspectc}, 1)
	CXX = ag++ -r repo.acp -v 0 ${ASPECTC_HOST_FLAGS} --c_compiler g++ -p . --Xcompiler
endif


LIB=-L/usr/lib/ -lm -pthread

DEP=kernel.cpp kernel.h main.cpp support/common.h support/setup.h support/timer.h 
SRC=main.cpp kernel.cpp
EXE=trns

all: trns

trns: ${SRC}
	$(CXX) -O3 $(CXX_FLAGS) ${CFLAGS} $(SRC) $(LIB) -o $(EXE) ${LDFLAGS}

trns_O0: ${SRC}
	$(CXX) $(CXX_FLAGS) $(SRC) $(LIB) -o $(EXE)_O0

trns_O2: ${SRC}
	$(CXX) -O2 $(CXX_FLAGS) $(SRC) $(LIB) -o $(EXE)_O2

run: trns
	./trns -w 0 -r 1 -m 16 -n 8 -o 4096 -p 2556

clean:
	rm -f $(EXE) ${EXE}_O0 ${EXE}_O2

.PHONY: all run clean
