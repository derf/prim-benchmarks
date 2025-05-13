NR_DPUS ?= 1
NR_TASKLETS ?= 16
BL ?= 10
WITH_ALLOC_OVERHEAD ?= 0
WITH_LOAD_OVERHEAD ?= 0
WITH_FREE_OVERHEAD ?= 0

HOST_SOURCES := $(wildcard host/*.c)
DPU_SOURCES := $(wildcard dpu/*.c)

aspectc ?= 0
aspectc_timing ?= 0
dfatool_timing ?= 1

COMMON_FLAGS := -Wall -Wextra  -g -Iinclude -DBL=${BL}
HOST_FLAGS := ${COMMON_FLAGS} -O3 `dpu-pkg-config --cflags --libs dpu` -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS} -DWITH_ALLOC_OVERHEAD=${WITH_ALLOC_OVERHEAD} -DWITH_LOAD_OVERHEAD=${WITH_LOAD_OVERHEAD} -DWITH_FREE_OVERHEAD=${WITH_FREE_OVERHEAD} -DDFATOOL_TIMING=${dfatool_timing} -DASPECTC=${aspectc} -lm
DPU_FLAGS := ${COMMON_FLAGS} -O2 -DNR_TASKLETS=${NR_TASKLETS}

ifeq (${aspectc_timing}, 1)
	ASPECTC_HOST_FLAGS += -ainclude/dfatool_host_dpu.ah -ainclude/dfatool_host.ah
endif

ASPECTC_HOST_FLAGS ?= -a0

ifeq (${aspectc}, 1)
	HOST_CC = ag++ -r repo.acp -v 0 ${ASPECTC_HOST_FLAGS} --c_compiler ${UPMEM_HOME}/bin/clang++ -p . --Xcompiler
else
	HOST_FLAGS += -std=c11
endif

QUIET = @

ifdef verbose
	QUIET =
endif

all: bin/ts_host bin/ts_dpu

bin:
	${QUIET}mkdir -p bin

bin/ts_host: ${HOST_SOURCES} ${COMMON_INCLUDES} bin
	${QUIET}cp ../include/dfatool_host_dpu.ah include
	${QUIET}${HOST_CC} -o $@ ${HOST_SOURCES} ${HOST_FLAGS}
	${QUIET}rm -f include/dfatool_host_dpu.ah

bin/ts_dpu: ${DPU_SOURCES} ${COMMON_INCLUDES} bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ ${DPU_SOURCES}

clean:
	${QUIET}rm -rf bin

test: all
	${QUIET}bin/ts_host -n 131072

.PHONY: all clean test
