NR_DPUS ?= 1
NR_TASKLETS ?= 16
PROBLEM_SIZE ?= 8192 # original 2, wtf

COMMON_INCLUDES := support
HOST_TARGET := bin/bs_host
DPU_TARGET := bin/bs_dpu

HOST_SOURCES := $(wildcard host/*.c)
DPU_SOURCES := $(wildcard dpu/*.c)

.PHONY: all clean test

COMMON_FLAGS := -Wall -Wextra  -g -I${COMMON_INCLUDES}
HOST_FLAGS := ${COMMON_FLAGS} -std=c11 -O3 `dpu-pkg-config --cflags --libs dpu` -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS} -DPROBLEM_SIZE=${PROBLEM_SIZE}
DPU_FLAGS := ${COMMON_FLAGS} -O2 -DNR_TASKLETS=${NR_TASKLETS}

QUIET = @

ifdef verbose
	QUIET =
endif

all: ${HOST_TARGET} ${DPU_TARGET}

bin:
	${QUIET}mkdir -p bin

${HOST_TARGET}: ${HOST_SOURCES} ${COMMON_INCLUDES} ${CONF}
	${QUIET}${CC} -o $@ ${HOST_SOURCES} ${HOST_FLAGS}

${DPU_TARGET}: ${DPU_SOURCES} ${COMMON_INCLUDES} ${CONF}
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ ${DPU_SOURCES}

clean:
	${QUIET}rm -rf bin

test: all
	${HOST_TARGET} -i 262144
