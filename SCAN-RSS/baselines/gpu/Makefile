aspectc ?= 0
aspectc_timing ?= 0
verbose ?= 0
TYPE ?= UINT64

NVCC ?= /usr/local/cuda-13.1/bin/nvcc

ifeq (${aspectc_timing}, 1)
	ASPECTC_HOST_FLAGS += -ainclude/dfatool_host_gpu.ah -ainclude/dfatool.ah
endif

ASPECTC_HOST_FLAGS ?= -a0

ifeq (${aspectc}, 1)
	CXX = ag++ -r repo.acp -v 0 ${ASPECTC_HOST_FLAGS} --c_compiler ${NVCC} -p . --Xcompiler --forward-unknown-opts
else
	CXX = ${NVCC}
endif

QUIET = @

ifeq (${verbose}, 1)
	QUIET =
endif

all: scan

scan: app_baseline.cu
	${QUIET}mkdir -p include
	${QUIET}cp ../../../include/dfatool_host_gpu.ah include
	${QUIET}${CXX} app_baseline.cu -I/usr/local/cuda-13.1/include -lm -o scan -D${TYPE}
	${QUIET}rm -f include/dfatool_host_gpu.ah

clean:
	${QUIET}rm -f scan

.PHONY: all clean
