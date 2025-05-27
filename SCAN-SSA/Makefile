NR_DPUS ?= 1
NR_TASKLETS ?= 16
BL ?= 10
TYPE ?= INT64
ENERGY ?= 0
UNROLL ?= 1

HOST_SOURCES := $(wildcard host/app.c)
OMP_SOURCES := $(wildcard host/omp.c)
DPU_SOURCES := $(wildcard dpu/*.c)

aspectc ?= 0
aspectc_timing ?= 0
dfatool_timing ?= 1

HOST_CC := ${CC}

COMMON_FLAGS = -Wall -Wextra -O2 -Iinclude -DNR_DPUS=${NR_DPUS} -DNR_TASKLETS=${NR_TASKLETS} -DBL=${BL} -D${TYPE} -DUNROLL=${UNROLL}
HOST_FLAGS = ${COMMON_FLAGS} `dpu-pkg-config --cflags --libs dpu` -DDFATOOL_TIMING=${dfatool_timing} -DASPECTC=${aspectc}
DPU_FLAGS = ${COMMON_FLAGS}

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

all: bin/dpu_code bin/host_code

bin:
	${QUIET}mkdir -p bin

bin/dpu_code: ${DPU_SOURCES} bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} ${DPU_SOURCES} -o $@

bin/host_code: ${HOST_SOURCES} bin
	${QUIET}cp ../include/dfatool_host_dpu.ah include
	${QUIET}${HOST_CC} -o $@ ${HOST_SOURCES} ${HOST_FLAGS}
	${QUIET}rm -f include/dfatool_host_dpu.ah

bin/omp_code: ${OMP_SOURCES}
	${QUIET}${CC} ${HOST_FLAGS} -fopenmp ${OMP_SOURCES} -o $@

clean:
	${QUIET}${RM} -f bin/dpu_code bin/host_code bin/omp_code

.PHONY: all clean
