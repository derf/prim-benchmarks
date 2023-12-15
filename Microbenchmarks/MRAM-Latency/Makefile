DPU_DIR := dpu
NR_TASKLETS ?= 16
BL ?= 8
NR_DPUS ?= 1
MEM ?= MRAM

COMMON_INCLUDES := support
HOST_SOURCES := $(wildcard host/*.c)

COMMON_FLAGS := -Wall -Wextra -g -I${COMMON_INCLUDES}
HOST_FLAGS := ${COMMON_FLAGS} -std=c11 -O3 `dpu-pkg-config --cflags --libs dpu` -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS} -DBL=${BL} -D${MEM}
DPU_FLAGS := ${COMMON_FLAGS} -O2 -flto -DNR_TASKLETS=${NR_TASKLETS} -DBL=${BL} -D${MEM}

QUIET = @

ifdef verbose
	QUIET =
endif

all: bin/host_code bin/dpu_code

bin:
	${QUIET}mkdir -p bin

bin/host_code: ${HOST_SOURCES} ${COMMON_INCLUDES} bin
	${QUIET}${CC} -o $@ ${HOST_SOURCES} ${HOST_FLAGS}

bin/dpu_code: dpu/copy.c ${COMMON_INCLUDES} bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ dpu/copy.c

clean:
	${QUIET}rm -rf bin

test: all
	${QUIET}bin/host_code

.PHONY: all clean test
