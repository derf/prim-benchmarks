NR_DPUS ?= 1
NR_TASKLETS ?= 16

COMMON_INCLUDES := support
HOST_SOURCES := $(wildcard host/*.c)
DPU_SOURCES := $(wildcard dpu/*.c)

COMMON_FLAGS := -Wall -Wextra -g -I${COMMON_INCLUDES}
HOST_FLAGS := ${COMMON_FLAGS} -std=c11 -O3 `dpu-pkg-config --cflags --libs dpu` -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS}
DPU_FLAGS := ${COMMON_FLAGS} -O2 -DNR_TASKLETS=${NR_TASKLETS}

QUIET = @

ifdef verbose
	QUIET =
endif

all: bin/host_code bin/dpu_code

bin:
	${QUIET}mkdir -p bin

bin/dpu_code: ${DPU_SOURCES} ${COMMON_INCLUDES} bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ ${DPU_SOURCES}

bin/host_code: ${HOST_SOURCES} ${COMMON_INCLUDES} bin
	${QUIET}${CC} -o $@ ${HOST_SOURCES} ${HOST_FLAGS}

clean:
	${QUIET}rm -rf bin

test: all
	bin/host_code

.PHONY: all clean test
