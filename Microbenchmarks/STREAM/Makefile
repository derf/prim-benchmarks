NR_TASKLETS ?= 16
BL ?= 10
NR_DPUS ?= 1
T ?= uint64_t
OP ?= copy
MEM ?= MRAM
UNROLL ?= 1
WITH_ALLOC_OVERHEAD ?= 0
WITH_LOAD_OVERHEAD ?= 0
WITH_FREE_OVERHEAD ?= 0
WITH_DPUINFO ?= 0
SDK_SINGLETHREADED ?= 0
TRANSFER ?= SERIAL

DPU_SOURCES = dpu/${OP}.c
HOST_SOURCES = $(wildcard host/*.c)

COMMON_INCLUDES = support
COMMON_FLAGS = -Wall -Wextra -O2 -I${COMMON_INCLUDES} -DNR_DPUS=${NR_DPUS} -DNR_TASKLETS=${NR_TASKLETS} -DBL=${BL} -DT=${T} -D${OP} -D${MEM} -DUNROLL=${UNROLL}
HOST_FLAGS = ${COMMON_FLAGS} -D_POSIX_C_SOURCE=200809L -std=c11 `dpu-pkg-config --cflags --libs dpu` -DWITH_ALLOC_OVERHEAD=${WITH_ALLOC_OVERHEAD} -DWITH_LOAD_OVERHEAD=${WITH_LOAD_OVERHEAD} -DWITH_FREE_OVERHEAD=${WITH_FREE_OVERHEAD} -DWITH_DPUINFO=${WITH_DPUINFO} -DSDK_SINGLETHREADED=${SDK_SINGLETHREADED} -D${TRANSFER}
DPU_FLAGS = ${COMMON_FLAGS} -flto

QUIET = @

ifdef verbose
	QUIET =
endif

all: bin/dpu_code bin/host_code

bin:
	${QUIET}mkdir -p bin

bin/dpu_code: ${DPU_SOURCES} bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} ${DPU_SOURCES} -o $@

bin/host_code: ${HOST_SOURCES} bin
	${QUIET}${CC} ${HOST_SOURCES} ${HOST_FLAGS} -o $@

clean:
	${QUIET}${RM} -f bin/dpu_code bin/host_code

info:
	@${CC} --version
	@dpu-upmem-dpurte-clang --version

.PHONY: all clean info
