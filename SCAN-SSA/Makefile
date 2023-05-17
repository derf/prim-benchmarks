NR_DPUS ?= 1
NR_TASKLETS ?= 16
BL ?= 10
TYPE ?= INT64
ENERGY ?= 0
UNROLL ?= 1

HOST_SOURCES := $(wildcard host/app.c)
OMP_SOURCES := $(wildcard host/omp.c)
DPU_SOURCES := $(wildcard dpu/*.c)

COMMON_INCLUDES := support
COMMON_FLAGS = -Wall -Wextra -O2 -I${COMMON_INCLUDES} -DNR_DPUS=${NR_DPUS} -DNR_TASKLETS=${NR_TASKLETS} -DBL=${BL} -D${TYPE} -DUNROLL=${UNROLL}
HOST_FLAGS = ${COMMON_FLAGS} -std=c11 `dpu-pkg-config --cflags --libs dpu`
DPU_FLAGS = ${COMMON_FLAGS}

QUIET = @

ifdef verbose
	QUIET =
endif

all: bin/dpu_code bin/host_code

bin:
	${QUIET}mkdir -p bin

bin/dpu_code: ${DPU_SOURCES}
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} ${DPU_SOURCES} -o $@

bin/host_code: ${HOST_SOURCES}
	${QUIET}${CC} ${HOST_FLAGS} ${HOST_SOURCES} -o $@

bin/omp_code: ${OMP_SOURCES}
	${QUIET}${CC} ${HOST_FLAGS} -fopenmp ${OMP_SOURCES} -o $@

clean:
	${QUIET}${RM} -f bin/dpu_code bin/host_code bin/omp_code

.PHONY: all clean
