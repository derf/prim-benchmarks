NR_TASKLETS ?= 16
NR_DPUS ?= 1

aspectc ?= 0
aspectc_timing ?= 0
dfatool_timing ?= 1

HOST_CC := ${CC}

COMMON_FLAGS := -Wall -Wextra -g -Iinclude
HOST_FLAGS := ${COMMON_FLAGS} -O3 `dpu-pkg-config --cflags --libs dpu` -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS} -DASPECTC=${aspectc} -DDFATOOL_TIMING=${dfatool_timing}
DPU_FLAGS := ${COMMON_FLAGS} -O2 -DNR_TASKLETS=${NR_TASKLETS}

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

bin/host_code: host/app.c include bin
	${QUIET}cp ../include/dfatool_host_dpu.ah include
	${QUIET}${HOST_CC} -o $@ host/app.c ${HOST_FLAGS}
	${QUIET}rm -f include/dfatool_host_dpu.ah

bin/dpu_code: dpu/task.c include bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ dpu/task.c

clean:
	${QUIET}rm -rf bin

test: all
	${QUIET}bin/host_code

.PHONY: all clean test
