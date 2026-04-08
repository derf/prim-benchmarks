NR_TASKLETS ?= 16
BL ?= 10
NR_RANKS ?= 1
NR_DPUS ?= 0
TYPE ?= uint64_t

dfatool_timing ?= 1
numa ?= 0

COMMON_FLAGS := -Wall -Wextra -g -Iinclude -DT=${TYPE} -std=c11
HOST_FLAGS := ${COMMON_FLAGS} -march=native -O2 `dpu-pkg-config --cflags --libs dpu` -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS} -DNR_RANKS=${NR_RANKS} -DBL=${BL} -DDFATOOL_TIMING=${dfatool_timing} -DNUMA=${numa}
DPU_FLAGS := ${COMMON_FLAGS} -O2 -DNR_TASKLETS=${NR_TASKLETS} -DBL=${BL}

ifeq (${numa}, 1)
	HOST_FLAGS += -lnuma
endif

QUIET = @

ifdef verbose
	QUIET =
endif

all: bin/host_code bin/dpu_code

bin:
	${QUIET}mkdir -p bin

bin/host_code: host/app.c include bin
	${QUIET}${CC} -o $@ host/app.c ${HOST_FLAGS}

bin/dpu_code: dpu/task.c include bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ dpu/task.c

clean:
	${QUIET}rm -rf bin

test: all
	${QUIET}bin/host_code

.PHONY: all clean test
