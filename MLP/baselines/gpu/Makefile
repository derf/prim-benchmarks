aspectc ?= 0
aspectc_timing ?= 0
verbose ?= 0

NR_THREADS ?= 128

NVCC ?= /usr/local/cuda-13.1/bin/nvcc
CXX = g++

COMMON_FLAGS := -I/usr/local/cuda/include -O2 -lm -DNR_THREADS=${NR_THREADS}
COMMON_FLAGS += -DASPECTC=${aspectc}

ifeq (${aspectc_timing}, 1)
	ASPECTC_HOST_FLAGS += -ainclude/dfatool_host_gpu.ah -ainclude/dfatool.ah
endif

ASPECTC_HOST_FLAGS ?= -a0

ifeq (${aspectc}, 1)
	CXX = ag++ -r repo.acp -v 0 ${ASPECTC_HOST_FLAGS} --c_compiler g++ -p . --Xcompiler
endif

QUIET = @

ifeq (${verbose}, 1)
	QUIET =
endif

all: mlp

kernel.o: kernel.cu
	${QUIET}${NVCC} ${COMMON_FLAGS} $^ -c -o $@

app.o: app.cc
	${QUIET}mkdir -p include
	${QUIET}cp ../../../include/dfatool_host_gpu.ah include
	${QUIET}${CXX} ${COMMON_FLAGS} $^ -c -o $@
	${QUIET}rm -f include/dfatool_host_gpu.ah

mlp: app.o kernel.o
	${QUIET}${NVCC} $^ -o $@

clean:
	${QUIET}rm -f mlp

.PHONY: all clean
