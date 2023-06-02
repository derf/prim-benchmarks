NR_TASKLETS ?= 16
NR_DPUS ?= 1

COMMON_INCLUDES := support
HOST_SOURCES := $(wildcard host/*.c)
DPU_SOURCES := $(wildcard dpu/*.c)
CPU_BASE_SOURCES := $(wildcard baselines/cpu/*.c)
GPU_BASE_SOURCES := $(wildcard baselines/gpu/*.cu)

COMMON_FLAGS := -Wall -Wextra -g -I${COMMON_INCLUDES}
HOST_FLAGS := ${COMMON_FLAGS} -std=c11 -O3 `dpu-pkg-config --cflags --libs dpu` -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS}
DPU_FLAGS := ${COMMON_FLAGS} -O2 -DNR_TASKLETS=${NR_TASKLETS}
CPU_BASE_FLAGS := -O3 -fopenmp
GPU_BASE_FLAGS := -O3

QUIET = @

ifdef verbose
	QUIET =
endif

all: bin/host_code bin/dpu_code

bin:
	${QUIET}mkdir -p bin

gpu: bin/gpu_baseline

bin/host_code: ${HOST_SOURCES} ${COMMON_INCLUDES} bin
	${QUIET}${CC} -o $@ ${HOST_SOURCES} ${HOST_FLAGS}

bin/dpu_code: ${DPU_SOURCES} ${COMMON_INCLUDES} bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ ${DPU_SOURCES}

bin/cpu_baseline: ${CPU_BASE_SOURCES}
	${QUIET}${CC} -o $@ ${CPU_BASE_SOURCES} ${CPU_BASE_FLAGS}

bin/gpu_baseline: ${GPU_BASE_SOURCES}
	${QUIET}nvcc -o $@ ${GPU_BASE_SOURCES} ${GPU_BASE_FLAGS}

clean:
	${QUIET}rm -rf bin

test: all
	${QUIET}bin/host_code

.PHONY: all clean test
