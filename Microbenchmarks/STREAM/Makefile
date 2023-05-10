NR_TASKLETS ?= 16
BL ?= 10
NR_DPUS ?= 1
T ?= uint64_t
OP ?= copy
MEM ?= MRAM
UNROLL ?= 1

DPU_SOURCES = dpu/${OP}.c
HOST_SOURCES = $(wildcard host/*.c)

COMMON_INCLUDES = support
COMMON_FLAGS = -Wall -Wextra -O2 -I${COMMON_INCLUDES} -DNR_DPUS=${NR_DPUS} -DNR_TASKLETS=${NR_TASKLETS} -DBL=${BL} -DT=${T} -D${OP} -D${MEM} -DUNROLL=${UNROLL}
HOST_FLAGS = ${COMMON_FLAGS} -std=c11 `dpu-pkg-config --cflags --libs dpu`
DPU_FLAGS = ${COMMON_FLAGS} -flto

all: bin/dpu_code bin/host_code

bin:
	mkdir -p bin

bin/dpu_code: ${DPU_SOURCES}
	dpu-upmem-dpurte-clang ${DPU_FLAGS} ${DPU_SOURCES} -o $@

bin/host_code: ${HOST_SOURCES}
	${CC} ${HOST_FLAGS} ${HOST_SOURCES} -o $@

clean:
	${RM} -f bin/dpu_code bin/host_code

.PHONY: all clean
