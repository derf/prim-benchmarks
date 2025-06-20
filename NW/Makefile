NR_TASKLETS ?= 13
BL ?= 1024
BL_IN ?= 4
NR_DPUS ?= 1
ENERGY ?= 0

HOST_SOURCES := $(wildcard host/*.c)
DPU_SOURCES := $(wildcard dpu/*.c)

aspectc ?= 0
aspectc_timing ?= 0

HOST_CC := ${CC}

COMMON_FLAGS := -Wall -Wextra -g -Iinclude -DNR_TASKLETS=${NR_TASKLETS} -DNR_DPUS=${NR_DPUS} -DBL=${BL}
HOST_FLAGS := ${COMMON_FLAGS} -O3 `dpu-pkg-config --cflags --libs dpu` -DENERGY=${ENERGY} -DASPECTC=${aspectc}
DPU_FLAGS := ${COMMON_FLAGS} -O2 -DBL_IN=${BL_IN}

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

ifdef verbose
	QUIET =
endif

all: bin/nw_host bin/nw_dpu

bin:
	${QUIET}mkdir -p bin

bin/nw_host: ${HOST_SOURCES} include bin
	${QUIET}cp ../include/dfatool_host_dpu.ah include
	${QUIET}${HOST_CC} -o $@ ${HOST_SOURCES} ${HOST_FLAGS}
	${QUIET}rm -f include/dfatool_host_dpu.ah

bin/nw_dpu: ${DPU_SOURCES} include bin
	${QUIET}dpu-upmem-dpurte-clang ${DPU_FLAGS} -o $@ ${DPU_SOURCES}

clean:
	$(RM) -r $(BUILDDIR)

test: all
	bin/nw_host

.PHONY: all clean test
