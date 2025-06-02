NR_DPUS ?= 1
NR_TASKLETS ?= 16
BL ?= 10
TYPE ?= INT32
ENERGY ?= 0
WITH_ALLOC_OVERHEAD ?= 0
WITH_LOAD_OVERHEAD ?= 0
WITH_FREE_OVERHEAD ?= 0
WITH_DPUINFO ?= 0

HOST_SOURCES := $(wildcard host/*.c)
DPU_SOURCES := $(wildcard dpu/*.c)

aspectc ?= 0
aspectc_timing ?= 0
dfatool_timing ?= 1

HOST_CC := ${CC}

COMMON_FLAGS := -Wall -Wextra -g -Iinclude
HOST_FLAGS := ${COMMON_FLAGS} -O3 `dpu-pkg-config --cflags --libs dpu` -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS} -DBL=${BL} -D${TYPE} -DENERGY=${ENERGY} -DWITH_ALLOC_OVERHEAD=${WITH_ALLOC_OVERHEAD} -DWITH_LOAD_OVERHEAD=${WITH_LOAD_OVERHEAD} -DWITH_FREE_OVERHEAD=${WITH_FREE_OVERHEAD} -DWITH_DPUINFO=${WITH_DPUINFO} -DDFATOOL_TIMING=${dfatool_timing} -DASPECTC=${aspectc}
DPU_FLAGS := ${COMMON_FLAGS} -O2 -DNR_TASKLETS=${NR_TASKLETS} -DBL=${BL} -D${TYPE}

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

ifeq (${verbose}, 1)
	QUIET =
endif

all: bin/host_code bin/dpu_code

bin:
	${QUIET}mkdir -p bin

# cp/rm are needed to work around AspectC++ not liking symlinks
bin/host_code: ${HOST_SOURCES} include bin
	${QUIET}cp ../include/dfatool_host_dpu.ah include
	${QUIET}${HOST_CC} -o $@ ${HOST_SOURCES} ${HOST_FLAGS}
	${QUIET}rm -f include/dfatool_host_dpu.ah

bin/dpu_code: ${DPU_SOURCES} include bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ ${DPU_SOURCES}

clean:
	${QUIET}rm -rf bin

test: all
	${QUIET}bin/host_code

.PHONY: all clean test
